/**
 * HearthKeep Room Monitor Node Firmware
 * 
 * MCU: nRF52833 (BLE 5.0, 64MHz Cortex-M4)
 * Radar: Infineon BGT60TR13C (60GHz FMCW, 1Tx/3Rx)
 * Env: BME688 (temp, humidity, pressure, VOC/IAQ)
 * Light: TSL25911 (ambient lux)
 * Radio: SX1261 (868MHz Sub-GHz mesh client)
 * 
 * Responsibilities:
 * - Continuous presence detection via mmWave radar (100ms cycle)
 * - Fall detection with local TFLite Micro classifier
 * - Environment monitoring (temp, humidity, IAQ, light)
 * - Mesh TDMA transmission to hub
 * - Self-calibration on power-up
 * - Ultra-low-power idle between readings
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <math.h>

#include "../common/mesh_protocol.h"

LOG_MODULE_REGISTER(hk_room_monitor, LOG_LEVEL_INF);

/* ========================================================================
 * HARDWARE DEFINITIONS
 * ======================================================================== */

/* Status LED (RGB) */
#define LED_R_NODE  DT_ALIAS(led0)
#define LED_G_NODE  DT_ALIAS(led1)
#define LED_B_NODE  DT_ALIAS(led2)
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_NODE, gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_NODE, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

/* Setup/Pairing Button */
#define BTN_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec setup_btn = GPIO_DT_SPEC_GET(BTN_NODE, gpios);

/* BGT60TR13C Radar I2C */
#define RADAR_I2C_DEV DT_NODELABEL(i2c0)
#define BGT60TR13C_ADDR 0x57

/* BME688 Environment I2C (shared bus) */
#define ENV_I2C_DEV DT_NODELABEL(i2c0)
#define BME688_ADDR 0x77

/* TSL25911 Light Sensor I2C */
#define LIGHT_I2C_DEV DT_NODELABEL(i2c0)
#define TSL25911_ADDR 0x29

/* SX1261 Radio SPI */
#define RADIO_SPI_DEV DT_NODELABEL(spi0)

/* Radar interrupt */
#define RADAR_IRQ_NODE DT_ALIAS(radar_irq)
static const struct gpio_dt_spec radar_irq = GPIO_DT_SPEC_GET(RADAR_IRQ_NODE, gpios);
static struct gpio_callback radar_irq_cb;

/* Radio interrupt */
#define RADIO_IRQ_NODE DT_ALIAS(radio_irq)
static const struct gpio_dt_spec radio_irq = GPIO_DT_SPEC_GET(RADIO_IRQ_NODE, gpios);

/* Supply voltage ADC */
#define VBAT_SENSE_DT DT_ALIAS(vbat_sense)

/* ========================================================================
 * BGT60TR13C RADAR DRIVER
 * ======================================================================== */

/* BGT60TR13C Register Map */
#define BGT_REG_CHIP_ID        0x00
#define BGT_REG_FW_VERSION     0x02
#define BGT_REG_MODE           0x10
#define BGT_REG_RADAR_CONFIG   0x11
#define BGT_REG_FRAME_CONFIG   0x12
#define BGT_REG_THRESHOLD      0x20
#define BGT_REG_MOTION_FLAGS   0x30
#define BGT_REG_TARGET_COUNT   0x31
#define BGT_REG_TARGET_INFO    0x32
#define BGT_REG_DISTANCE       0x34
#define BGT_REG_VELOCITY       0x36
#define BGT_REG_ANGLE          0x38
#define BGT_REG_INT_CONFIG     0x40
#define BGT_REG_SYSTEM_CONFIG  0x50

/* Radar modes */
typedef enum {
    BGT_MODE_IDLE          = 0x00,
    BGT_MODE_PRESENCE      = 0x01,  /* Low-power presence detection */
    BGT_MODE_MOTION        = 0x02,  /* Motion detection */
    BGT_MODE_POINT_CLOUD   = 0x03,  /* Full point cloud for fall detection */
    BGT_MODE_VITAL_SIGNS   = 0x04,  /* Breathing/heart micro-Doppler */
} bgt_mode_t;

typedef struct {
    float distance_m;
    float velocity_ms;
    float angle_deg;
    float rcs_dbm;
    float elevation_deg;
} bgt_target_t;

typedef struct {
    uint8_t target_count;
    bgt_target_t targets[4];
    float fall_probability;
    hk_position_class_t position_class;
    float movement_index;
    float presence_probability;
    bool motion_detected;
} bgt_radar_frame_t;

static bgt_radar_frame_t g_radar_frame;

/* I2C write to BGT60TR13C */
static int bgt_write_reg(const struct device *i2c, uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t buf[32];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return i2c_write(i2c, buf, len + 1, BGT60TR13C_ADDR);
}

/* I2C read from BGT60TR13C */
static int bgt_read_reg(const struct device *i2c, uint8_t reg, uint8_t *data, uint8_t len)
{
    return i2c_write_read(i2c, BGT60TR13C_ADDR, &reg, 1, data, len);
}

static int bgt_init(const struct device *i2c)
{
    LOG_INF("Initializing BGT60TR13C radar...");
    
    uint8_t chip_id[2];
    int ret = bgt_read_reg(i2c, BGT_REG_CHIP_ID, chip_id, 2);
    if (ret != 0) {
        LOG_ERR("Failed to read BGT60TR13C chip ID");
        return ret;
    }
    LOG_INF("BGT60TR13C chip ID: 0x%02X%02X", chip_id[0], chip_id[1]);
    
    /* Configure for presence detection mode initially */
    uint8_t config[] = {
        0x01,  /* Mode: presence detection */
        0x00,  /* Normal sensitivity */
        0x00,  /* Default thresholds */
    };
    ret = bgt_write_reg(i2c, BGT_REG_RADAR_CONFIG, config, sizeof(config));
    if (ret != 0) {
        LOG_ERR("Failed to configure radar");
        return ret;
    }
    
    /* Enable interrupts for motion and presence */
    uint8_t int_config[] = {
        0x03,  /* Enable motion + presence interrupts */
        0x00,
    };
    bgt_write_reg(i2c, BGT_REG_INT_CONFIG, int_config, sizeof(int_config));
    
    LOG_INF("BGT60TR13C initialized - starting in presence mode");
    return 0;
}

static int bgt_set_mode(const struct device *i2c, bgt_mode_t mode)
{
    uint8_t mode_byte = (uint8_t)mode;
    return bgt_write_reg(i2c, BGT_REG_MODE, &mode_byte, 1);
}

static int bgt_read_targets(const struct device *i2c, bgt_radar_frame_t *frame)
{
    uint8_t target_count;
    int ret = bgt_read_reg(i2c, BGT_REG_TARGET_COUNT, &target_count, 1);
    if (ret != 0) return ret;
    
    frame->target_count = target_count > 4 ? 4 : target_count;
    
    for (int i = 0; i < frame->target_count; i++) {
        uint8_t target_data[8];
        ret = bgt_read_reg(i2c, BGT_REG_TARGET_INFO + (i * 8), target_data, 8);
        if (ret != 0) return ret;
        
        /* Parse target data (distance, velocity, angle, RCS) */
        int16_t raw_dist = (int16_t)((target_data[0] << 8) | target_data[1]);
        int16_t raw_vel = (int16_t)((target_data[2] << 8) | target_data[3]);
        int16_t raw_angle = (int16_t)((target_data[4] << 8) | target_data[5]);
        
        frame->targets[i].distance_m = raw_dist * 0.01f;   /* cm to m */
        frame->targets[i].velocity_ms = raw_vel * 0.01f;    /* cm/s to m/s */
        frame->targets[i].angle_deg = raw_angle * 0.1f;     /* 0.1 deg resolution */
    }
    
    return 0;
}

/* ========================================================================
 * FALL DETECTION (Simplified local classifier)
 * In production, this would use TFLite Micro with the trained model
 * ======================================================================== */

typedef struct {
    float distance_history[8];    /* Last 8 distance readings */
    float velocity_history[8];    /* Last 8 velocity readings */
    float rcs_history[8];         /* Last 8 RCS readings */
    int history_idx;
    bool history_full;
    uint32_t last_fall_ms;        /* Debounce: last fall detection time */
} fall_detector_t;

static fall_detector_t g_fall_detector;

static hk_position_class_t classify_position(const bgt_radar_frame_t *frame)
{
    if (frame->target_count == 0) {
        return HK_POS_ABSENT;
    }
    
    /* Get primary target */
    float distance = frame->targets[0].distance_m;
    float velocity = frame->targets[0].velocity_ms;
    float rcs = frame->targets[0].rcs_dbm;
    
    /* Simple heuristic classifier */
    /* In production, this would be the TFLite Micro model */
    
    /* If vertical velocity is very high and negative, person is falling */
    if (velocity < -1.5f) {
        return HK_POS_FALLING;
    }
    
    /* If very close to ground (< 0.3m from radar at 1.5m height), likely lying */
    if (distance < 0.3f && fabsf(velocity) < 0.2f) {
        return HK_POS_LYING;
    }
    
    /* If close to radar height (0.8-1.8m), likely standing */
    if (distance > 0.8f && distance < 2.0f && fabsf(velocity) < 0.5f) {
        return HK_POS_STANDING;
    }
    
    /* If at mid-height (0.4-0.8m), likely sitting */
    if (distance > 0.4f && distance < 0.8f && fabsf(velocity) < 0.3f) {
        return HK_POS_SITTING;
    }
    
    return HK_POS_UNKNOWN;
}

static float calculate_fall_probability(const bgt_radar_frame_t *frame,
                                          const fall_detector_t *fd)
{
    if (frame->target_count == 0) return 0.0f;
    
    float prob = 0.0f;
    
    /* Check for rapid vertical velocity change (falling) */
    if (frame->targets[0].velocity_ms < -1.0f) {
        prob += 0.4f;
    }
    if (frame->targets[0].velocity_ms < -2.0f) {
        prob += 0.3f;
    }
    
    /* Check for position close to floor */
    if (frame->targets[0].distance_m < 0.3f) {
        prob += 0.2f;
    }
    
    /* Check for sudden drop in height from history */
    if (fd->history_full) {
        float prev_dist = fd->distance_history[(fd->history_idx - 3) & 7];
        float curr_dist = frame->targets[0].distance_m;
        if (prev_dist > 0.8f && curr_dist < 0.3f) {
            prob += 0.3f;  /* Sudden height drop = likely fall */
        }
    }
    
    /* Clamp */
    if (prob > 1.0f) prob = 1.0f;
    return prob;
}

static void update_fall_history(fall_detector_t *fd, const bgt_radar_frame_t *frame)
{
    if (frame->target_count > 0) {
        fd->distance_history[fd->history_idx] = frame->targets[0].distance_m;
        fd->velocity_history[fd->history_idx] = frame->targets[0].velocity_ms;
        fd->rcs_history[fd->history_idx] = frame->targets[0].rcs_dbm;
    }
    fd->history_idx = (fd->history_idx + 1) & 7;
    if (fd->history_idx == 0) fd->history_full = true;
}

/* ========================================================================
 * BME688 ENVIRONMENT SENSOR DRIVER
 * ======================================================================== */

typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    float iaq_index;
} bme688_data_t;

static bme688_data_t g_env_data;

static int bme688_init(const struct device *i2c)
{
    LOG_INF("Initializing BME688...");
    
    /* Software reset */
    uint8_t reset_cmd = 0xB6;
    i2c_write(i2c, &reset_cmd, 1, BME688_ADDR);
    k_msleep(10);
    
    /* Set oversampling: temp×8, humidity×2, pressure×4 */
    uint8_t ctrl_hum = 0x02;  /* Humidity oversampling ×2 */
    uint8_t ctrl_meas = 0x54;  /* Temp×8, Pressure×4, Mode=Sleep */
    uint8_t ctrl_gas = 0x00;   /* Disable gas wait for now */
    
    i2c_reg_write_byte(i2c, BME688_ADDR, 0x72, ctrl_hum);
    i2c_reg_write_byte(i2c, BME688_ADDR, 0x74, ctrl_meas);
    i2c_reg_write_byte(i2c, BME688_ADDR, 0x75, ctrl_gas);
    
    LOG_INF("BME688 initialized");
    return 0;
}

static int bme688_read(const struct device *i2c, bme688_data_t *data)
{
    /* Trigger one-shot measurement */
    uint8_t ctrl_meas = 0x55;  /* Mode = Forced */
    i2c_reg_write_byte(i2c, BME688_ADDR, 0x74, ctrl_meas);
    
    k_msleep(100);  /* Wait for measurement */
    
    /* Read raw data registers */
    uint8_t raw[8];
    int ret = i2c_write_read(i2c, BME688_ADDR, 
                              (uint8_t[]){0x1D}, 1, raw, 8);
    if (ret != 0) return ret;
    
    /* Convert raw data (simplified - real driver uses BME680 compensation) */
    int32_t raw_temp = (raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4);
    int32_t raw_press = (raw[3] << 12) | (raw[4] << 4) | (raw[5] >> 4);
    int32_t raw_hum = (raw[6] << 8) | raw[7];
    
    /* Simplified conversion (would use BOSCH compensation library in production) */
    data->temperature_c = 23.0f + (raw_temp / 16384.0f - 1.0f) * 5.0f;
    data->pressure_hpa = 1013.25f + (raw_press / 16384.0f - 1.0f) * 50.0f;
    data->humidity_pct = 50.0f + (raw_hum / 1024.0f - 1.0f) * 30.0f;
    data->iaq_index = 100.0f;  /* Default, would be calculated from gas resistance */
    
    return 0;
}

/* ========================================================================
 * TSL25911 LIGHT SENSOR DRIVER
 * ======================================================================== */

static float g_light_lux;

static int tsl25911_init(const struct device *i2c)
{
    LOG_INF("Initializing TSL25911...");
    
    /* Enable the sensor */
    uint8_t enable_cmd = 0x03;  /* Power on + ADC enable */
    i2c_reg_write_byte(i2c, TSL25911_ADDR, 0x01, enable_cmd);
    
    /* Set integration time 200ms, gain medium */
    uint8_t config = 0x12;  /* 200ms integration, medium gain */
    i2c_reg_write_byte(i2c, TSL25911_ADDR, 0x01, config);
    
    k_msleep(250);  /* Wait for first integration */
    
    LOG_INF("TSL25911 initialized");
    return 0;
}

static int tsl25911_read(const struct device *i2c, float *lux)
{
    uint8_t data[4];
    int ret = i2c_write_read(i2c, TSL25911_ADDR,
                              (uint8_t[]){0x14}, 1, data, 4);
    if (ret != 0) return ret;
    
    uint16_t ch0 = (data[1] << 8) | data[0];  /* Full spectrum */
    uint16_t ch1 = (data[3] << 8) | data[2];  /* Infrared */
    
    /* Calculate lux (simplified) */
    if (ch0 == 0) {
        *lux = 0.0f;
    } else {
        float ratio = (float)ch1 / (float)ch0;
        if (ratio <= 0.5f) {
            *lux = (ch0 - ch1) * 16.0f;  /* Medium gain factor */
        } else if (ratio <= 0.61f) {
            *lux = (ch0 * 1.3f - ch1 * 2.5f) * 16.0f;
        } else {
            *lux = 0.0f;  /* Saturated */
        }
    }
    
    return 0;
}

/* ========================================================================
 * SX1261 RADIO DRIVER (Simplified)
 * ======================================================================== */

static int radio_init(void)
{
    LOG_INF("Initializing SX1261 radio...");
    /* In production: full SX1261 driver with SPI commands */
    LOG_INF("SX1261 initialized on 868MHz");
    return 0;
}

static int radio_transmit_packet(const hk_packet_t *pkt)
{
    uint8_t tx_buf[HK_MAX_PACKET_LEN];
    uint16_t tx_len = hk_packet_serialize(pkt, tx_buf, sizeof(tx_buf));
    if (tx_len == 0) return -1;
    
    /* In production: SX1261 TX command */
    LOG_DBG("TX: %s (len=%d)", hk_type_str(pkt->type), tx_len);
    return 0;
}

/* ========================================================================
 * APPLICATION STATE
 * ======================================================================== */

typedef struct {
    uint8_t node_id;
    uint8_t tdma_slot;
    uint32_t last_env_read_ms;
    uint32_t last_mesh_tx_ms;
    uint32_t uptime_ms;
    bgt_mode_t current_radar_mode;
    bool calibrated;
    uint8_t sensitivity;  /* 0=low, 1=normal, 2=high */
    uint8_t reporting_interval_s;  /* Seconds between mesh reports */
    hk_radar_data_t last_radar;
    hk_env_data_t last_env;
} room_monitor_state_t;

static room_monitor_state_t g_rm;

/* K-thread stacks */
#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(radar_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(env_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(mesh_stack, STACK_SIZE);

static struct k_thread radar_thread_data;
static struct k_thread env_thread_data;
static struct k_thread mesh_thread_data;

/* Radar interrupt callback */
static bool g_radar_data_ready = false;

static void radar_irq_handler(const struct device *dev,
                               struct gpio_callback *cb,
                               uint32_t pins)
{
    g_radar_data_ready = true;
}

/* ========================================================================
 * RADAR THREAD — Continuous presence detection and fall detection
 * ======================================================================== */

static void radar_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Radar thread started");
    
    const struct device *i2c = DEVICE_DT_GET(RADAR_I2C_DEV);
    
    while (1) {
        uint32_t now = k_uptime_get_32();
        
        /* Check if radar has data ready */
        if (g_radar_data_ready) {
            g_radar_data_ready = false;
            
            /* Read target data from radar */
            bgt_read_targets(i2c, &g_radar_frame);
            
            /* Classify position */
            g_radar_frame.position_class = classify_position(&g_radar_frame);
            
            /* Update fall detection history */
            update_fall_history(&g_fall_detector, &g_radar_frame);
            
            /* Calculate fall probability */
            g_radar_frame.fall_probability = 
                calculate_fall_probability(&g_radar_frame, &g_fall_detector);
            
            /* Calculate movement index */
            if (g_radar_frame.target_count > 0) {
                g_radar_frame.movement_index = 
                    fabsf(g_radar_frame.targets[0].velocity_ms) / 2.0f;
                if (g_radar_frame.movement_index > 1.0f) 
                    g_radar_frame.movement_index = 1.0f;
            } else {
                g_radar_frame.movement_index = 0.0f;
            }
            
            /* Update last radar data */
            g_rm.last_radar.presence_count = g_radar_frame.target_count;
            g_rm.last_radar.position_class = g_radar_frame.position_class;
            g_rm.last_radar.fall_probability = g_radar_frame.fall_probability;
            g_rm.last_radar.movement_index = g_radar_frame.movement_index;
            if (g_radar_frame.target_count > 0) {
                g_rm.last_radar.distance_m = g_radar_frame.targets[0].distance_m;
                g_rm.last_radar.velocity_ms = g_radar_frame.targets[0].velocity_ms;
            }
            g_rm.last_radar.radar_timestamp = now;
            g_rm.last_radar.confidence = 
                g_radar_frame.presence_probability > 0.7f ? 95 : 
                g_radar_frame.presence_probability > 0.4f ? 70 : 30;
            
            /* Switch radar mode based on presence */
            if (g_radar_frame.target_count > 0 && 
                g_rm.current_radar_mode == BGT_MODE_PRESENCE) {
                /* Someone detected, switch to full point cloud for fall detection */
                bgt_set_mode(i2c, BGT_MODE_POINT_CLOUD);
                g_rm.current_radar_mode = BGT_MODE_POINT_CLOUD;
                LOG_INF("Switched to point cloud mode (person detected)");
            } else if (g_radar_frame.target_count == 0 &&
                       g_rm.current_radar_mode == BGT_MODE_POINT_CLOUD) {
                /* No one present, switch back to low-power presence detection */
                bgt_set_mode(i2c, BGT_MODE_PRESENCE);
                g_rm.current_radar_mode = BGT_MODE_PRESENCE;
                LOG_INF("Switched to presence mode (no one detected)");
            }
            
            /* Check for fall - immediate alert */
            if (g_radar_frame.fall_probability > HK_FALL_PROBABILITY_THRESHOLD) {
                LOG_WRN("FALL DETECTED! Probability: %.2f, Position: %s",
                       g_radar_frame.fall_probability,
                       hk_position_str(g_radar_frame.position_class));
                
                /* Send FALL_ALERT immediately (bypasses TDMA) */
                hk_packet_t pkt;
                hk_packet_init(&pkt, g_rm.node_id, HK_NODE_ID_HUB, HK_TYPE_FALL_ALERT);
                
                hk_fall_alert_t fall;
                fall.room_id = g_rm.node_id;
                fall.position_class = g_radar_frame.position_class;
                fall.fall_probability = g_radar_frame.fall_probability;
                fall.impact_velocity = g_radar_frame.target_count > 0 ? 
                    fabsf(g_radar_frame.targets[0].velocity_ms) : 0.0f;
                fall.timestamp = now / 1000;
                fall.verification_attempts = 0;
                fall.verified = g_radar_frame.fall_probability > 0.95f ? 1 : 0;
                
                memcpy(pkt.payload, &fall, sizeof(fall));
                pkt.length = HK_HEADER_LEN + sizeof(fall) + HK_CRC_LEN;
                radio_transmit_packet(&pkt);
                
                /* Flash red LED rapidly */
                for (int i = 0; i < 10; i++) {
                    gpio_pin_set_dt(&led_r, 1);
                    k_msleep(100);
                    gpio_pin_set_dt(&led_r, 0);
                    k_msleep(100);
                }
            }
        }
        
        /* Poll radar at 100ms intervals in presence mode, 10Hz in point cloud */
        uint32_t poll_interval = (g_rm.current_radar_mode == BGT_MODE_POINT_CLOUD) ? 100 : 1000;
        k_msleep(poll_interval);
    }
}

/* ========================================================================
 * ENVIRONMENT THREAD — Read temp, humidity, pressure, IAQ, light
 * ======================================================================== */

static void env_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Environment thread started");
    
    const struct device *i2c = DEVICE_DT_GET(ENV_I2C_DEV);
    
    bme688_init(i2c);
    tsl25911_init(i2c);
    
    while (1) {
        uint32_t now = k_uptime_get_32();
        
        /* Read environment data every 30 seconds */
        bme688_read(i2c, &g_env_data);
        tsl25911_read(i2c, &g_light_lux);
        
        /* Update environment data struct */
        g_rm.last_env.temperature_c = g_env_data.temperature_c;
        g_rm.last_env.humidity_pct = g_env_data.humidity_pct;
        g_rm.last_env.pressure_hpa = g_env_data.pressure_hpa;
        g_rm.last_env.iaq_index = g_env_data.iaq_index;
        g_rm.last_env.light_lux = g_light_lux;
        g_rm.last_env.room_id = g_rm.node_id;
        g_rm.last_env.occupancy = g_rm.last_radar.presence_count;
        
        LOG_INF("Env: %.1f°C %.0f%%RH %.0fhPa IAQ=%.0f Lux=%.0f",
                g_env_data.temperature_c, g_env_data.humidity_pct,
                g_env_data.pressure_hpa, g_env_data.iaq_index, g_light_lux);
        
        k_msleep(30000);  /* 30 second interval */
    }
}

/* ========================================================================
 * MESH THREAD — TDMA transmission to hub
 * ======================================================================== */

static void mesh_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Mesh thread started (slot=%d)", g_rm.tdma_slot);
    
    while (1) {
        uint32_t now = k_uptime_get_32();
        uint32_t frame_ms = now % HK_FRAME_DURATION_MS;
        uint32_t slot_start_ms = g_rm.tdma_slot * HK_SLOT_DURATION_MS;
        
        /* Wait for our TDMA slot */
        int32_t wait_ms = (int32_t)(slot_start_ms - frame_ms);
        if (wait_ms > 0) {
            k_msleep(wait_ms);
        }
        
        /* Transmit radar data */
        hk_packet_t radar_pkt;
        hk_packet_init(&radar_pkt, g_rm.node_id, HK_NODE_ID_HUB, HK_TYPE_RADAR_DATA);
        memcpy(radar_pkt.payload, &g_rm.last_radar, sizeof(hk_radar_data_t));
        radar_pkt.length = HK_HEADER_LEN + sizeof(hk_radar_data_t) + HK_CRC_LEN;
        radar_pkt.seq_num = g_rm.uptime_ms / 100;
        radio_transmit_packet(&radar_pkt);
        
        /* Transmit environment data every 30 seconds */
        if (now - g_rm.last_env_read_ms > 30000) {
            hk_packet_t env_pkt;
            hk_packet_init(&env_pkt, g_rm.node_id, HK_NODE_ID_HUB, HK_TYPE_ENV_DATA);
            memcpy(env_pkt.payload, &g_rm.last_env, sizeof(hk_env_data_t));
            env_pkt.length = HK_HEADER_LEN + sizeof(hk_env_data_t) + HK_CRC_LEN;
            env_pkt.seq_num = g_rm.uptime_ms / 30000;
            radio_transmit_packet(&env_pkt);
            g_rm.last_env_read_ms = now;
        }
        
        /* Sleep until next frame */
        k_msleep(HK_FRAME_DURATION_MS);
    }
}

/* ========================================================================
 * MAIN ENTRY POINT
 * ======================================================================== */

int main(void)
{
    LOG_INF("=== HearthKeep Room Monitor Starting ===");
    
    /* Initialize state */
    memset(&g_rm, 0, sizeof(g_rm));
    g_rm.node_id = 0x01;  /* Default, set during pairing */
    g_rm.tdma_slot = hk_slot_for_node(g_rm.node_id);
    g_rm.current_radar_mode = BGT_MODE_PRESENCE;
    g_rm.sensitivity = 1;  /* Normal */
    g_rm.reporting_interval_s = 1;  /* 1 second */
    g_rm.calibrated = false;
    
    memset(&g_fall_detector, 0, sizeof(g_fall_detector));
    memset(&g_radar_frame, 0, sizeof(g_radar_frame));
    
    /* Initialize GPIOs */
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&setup_btn, GPIO_INPUT);
    
    /* Startup LED animation */
    for (int i = 0; i < 3; i++) {
        gpio_pin_set_dt(&led_g, 1);
        k_msleep(200);
        gpio_pin_set_dt(&led_g, 0);
        k_msleep(200);
    }
    
    /* Initialize I2C bus */
    const struct device *i2c = DEVICE_DT_GET(RADAR_I2C_DEV);
    if (!device_is_ready(i2c)) {
        LOG_ERR("I2C device not ready");
        while (1) {
            gpio_pin_set_dt(&led_r, 1);
            k_msleep(100);
            gpio_pin_set_dt(&led_r, 0);
            k_msleep(100);
        }
    }
    
    /* Initialize radar */
    if (bgt_init(i2c) != 0) {
        LOG_ERR("Failed to initialize radar!");
        while (1) {
            gpio_pin_set_dt(&led_r, 1);
            k_msleep(500);
            gpio_pin_set_dt(&led_r, 0);
            k_msleep(500);
        }
    }
    
    /* Configure radar interrupt */
    gpio_pin_configure_dt(&radar_irq, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&radar_irq, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&radar_irq_cb, radar_irq_handler, BIT(radar_irq.pin));
    gpio_add_callback(radar_irq.port, &radar_irq_cb);
    
    /* Initialize radio */
    radio_init();
    
    /* Start radar in presence detection mode */
    bgt_set_mode(i2c, BGT_MODE_PRESENCE);
    g_rm.current_radar_mode = BGT_MODE_PRESENCE;
    
    /* Start worker threads */
    k_thread_create(&radar_thread_data, radar_stack, STACK_SIZE,
                    radar_thread_fn, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);  /* High priority for fall detection */
    
    k_thread_create(&env_thread_data, env_stack, STACK_SIZE,
                    env_thread_fn, NULL, NULL, NULL,
                    3, 0, K_NO_WAIT);
    
    k_thread_create(&mesh_thread_data, mesh_stack, STACK_SIZE,
                    mesh_thread_fn, NULL, NULL, NULL,
                    4, 0, K_NO_WAIT);
    
    LOG_INF("Room Monitor running. Node ID: 0x%02X, Slot: %d", 
            g_rm.node_id, g_rm.tdma_slot);
    
    /* Main loop - status LED and self-test */
    while (1) {
        g_rm.uptime_ms = k_uptime_get_32();
        
        /* Pulse green LED to show we're alive */
        gpio_pin_set_dt(&led_g, 1);
        k_msleep(50);
        gpio_pin_set_dt(&led_g, 0);
        
        k_msleep(5000);  /* Blink every 5 seconds */
    }
    
    return 0;
}
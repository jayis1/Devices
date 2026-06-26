/*
 * BrewSync Fermenter Node - Main Firmware
 * STM32L476RG, bare-metal, low-power
 *
 * Reads: SG (tilt), temp, CO2, pressure, pH
 * Reports via Sub-GHz SX1262 to Hub every 5 min (normal) / 30 sec (active)
 *
 * Copyright (c) 2025 BrewSync. MIT License.
 */

#include <string.h>
#include <math.h>
#include "stm32l4xx.h"
#include "bsmp.h"
#include "bsmp_sensors.h"

/* ---- Pin definitions ---- */
#define PIN_SX1262_CS      0   /* PA0 - SPI CS for SX1262 */
#define PIN_SX1262_BUSY    1   /* PA1 - SX1262 busy */
#define PIN_SX1262_DIO1    2   /* PA2 - SX1262 DIO1 interrupt */
#define PIN_SX1262_RST     3   /* PA3 - SX1262 reset */
#define PIN_DS18B20        4   /* PA4 - OneWire data */
#define PIN_PH_POWER       5   /* PA5 - pH probe power enable */
#define PIN_LED            6   /* PB0 - Status LED */
#define PIN_BUTTON         7   /* PB1 - User button */

/* ---- Configuration ---- */
#define FW_VERSION          "1.2.0"
#define REPORT_INTERVAL_NORMAL_MS   (5UL * 60UL * 1000UL)  /* 5 minutes */
#define REPORT_INTERVAL_ACTIVE_MS   (30UL * 1000UL)         /* 30 seconds */
#define CO2_ACTIVE_THRESHOLD_PPM    500.0f  /* Above this = active fermentation */
#define BATTERY_LOW_MV              3300    /* 3.3V low battery */
#define SG_CAL_SLOPE                0.0038f /* Calibration: tilt angle → SG */
#define SG_CAL_OFFSET               1.0000f /* Calibration offset */

/* ---- Global state ---- */
typedef enum {
    STATE_INIT,
    STATE_LAG,
    STATE_ACTIVE,
    STATE_FINISHING,
    STATE_CONDITIONING,
    STATE_COMPLETE,
    STATE_ERROR
} ferment_state_t;

static struct {
    uint16_t node_addr;
    uint8_t  seq;
    ferment_state_t state;
    uint32_t report_interval_ms;
    float    target_sg;
    float    target_temp_c;
    bool     batch_active;
    uint32_t last_report_ms;
    uint32_t active_co2_count;
    uint32_t last_co2_high_ms;

    /* Calibration */
    float sg_slope;
    float sg_offset;

    /* AES key (provisioned during pairing) */
    uint8_t aes_key[16];
    bool   paired;
} g;

static bsmp_hal_t hal;

/* ---- Platform HAL (STM32L4 specific) ---- */
/* These would be implemented in hal_stm32l4.c */
extern int  hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
extern int  hal_i2c_write(uint8_t bus, uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
extern int  hal_spi_xfer(uint8_t bus, uint8_t cs, const uint8_t *tx, uint8_t *rx, uint16_t len);
extern void hal_delay_ms(uint32_t ms);
extern void hal_delay_us(uint32_t us);
extern int  hal_gpio_set(uint8_t pin, bool high);
extern int  hal_gpio_get(uint8_t pin, bool *high);
extern int  hal_adc_read(uint8_t channel, uint16_t *value);
extern void hal_enter_lowpower(uint32_t wake_ms);
extern uint32_t hal_get_ticks(void);

static void init_hal(void) {
    hal.i2c_read   = hal_i2c_read;
    hal.i2c_write  = hal_i2c_write;
    hal.spi_xfer   = hal_spi_xfer;
    hal.delay_ms   = hal_delay_ms;
    hal.delay_us   = hal_delay_us;
    hal.gpio_set   = hal_gpio_set;
    hal.gpio_get   = hal_gpio_get;
    hal.adc_read   = hal_adc_read;
}

/* ---- Sensor reading ---- */
static int read_all_sensors(bsmp_fermenter_telem_t *telem) {
    adxl362_t tilt;
    ds18b20_t temp;
    scd41_t   co2;
    ms5837_t  press;
    ezoph_t   ph;
    uint16_t  battery_mv;
    int rc;

    memset(telem, 0, sizeof(*telem));
    telem->sensor_status = 0;
    telem->timestamp = hal_get_ticks() / 1000; /* Simplified; RTC in production */

    /* Read tilt/SG */
    rc = adxl362_read(&hal, 0, PIN_SX1262_CS, &tilt);
    if (rc == 0 && tilt.valid) {
        telem->sg = tilt.sg;
        telem->sensor_status |= BSMP_SENS_SG_OK;
    } else {
        telem->sg = 0.0f;
    }

    /* Read temperature */
    rc = ds18b20_read(&hal, PIN_DS18B20, &temp);
    if (rc == 0 && temp.valid) {
        telem->temp_c = temp.temp_c;
        telem->sensor_status |= BSMP_SENS_TEMP_OK;
    } else {
        telem->temp_c = -999.0f;
    }

    /* Read CO2 */
    rc = scd41_read(&hal, 0, &co2);
    if (rc == 0 && co2.valid) {
        telem->co2_ppm = co2.co2_ppm;
        telem->sensor_status |= BSMP_SENS_CO2_OK;
    } else {
        telem->co2_ppm = 0.0f;
    }

    /* Read pressure */
    rc = ms5837_read(&hal, 0, &press);
    if (rc == 0 && press.valid) {
        telem->pressure_bar = press.pressure_bar;
        telem->sensor_status |= BSMP_SENS_PRESS_OK;
    } else {
        telem->pressure_bar = 0.0f;
    }

    /* Read pH */
    hal_gpio_set(PIN_PH_POWER, true);
    hal_delay_ms(500); /* Let pH probe stabilize */
    rc = ezoph_read(&hal, 0, &ph);
    hal_gpio_set(PIN_PH_POWER, false);
    if (rc == 0 && ph.valid) {
        /* Temperature compensate pH reading */
        if (telem->sensor_status & BSMP_SENS_TEMP_OK) {
            ezoph_temp_compensate(&hal, 0, telem->temp_c);
        }
        telem->ph = ph.ph;
        telem->sensor_status |= BSMP_SENS_PH_OK;
    } else {
        telem->ph = 0.0f;
    }

    /* Read battery */
    hal_adc_read(0, &battery_mv);
    telem->battery_mv = battery_mv;

    /* Check alarms */
    telem->flags = 0;
    if (g.batch_active && g.target_temp_c > 0.0f) {
        if (fabsf(telem->temp_c - g.target_temp_c) > 3.0f)
            telem->flags |= BSMP_FLAG_TEMP_ALARM;
    }
    if (g.batch_active && g.target_sg > 0.0f && telem->sg > 0.0f) {
        /* SG alarm if it goes UP during fermentation (infection/issue) */
        /* Simplified; real logic tracks SG trend */
    }
    if (telem->co2_ppm > 5000.0f)
        telem->flags |= BSMP_FLAG_CO2_ALARM;
    if (telem->ph > 0.0f && (telem->ph < 3.0f || telem->ph > 5.5f))
        telem->flags |= BSMP_FLAG_PH_ALARM;

    return 0;
}

/* ---- Fermentation state machine ---- */
static void update_ferment_state(const bsmp_fermenter_telem_t *telem) {
    if (!g.batch_active) {
        g.state = STATE_INIT;
        g.report_interval_ms = REPORT_INTERVAL_NORMAL_MS;
        return;
    }

    switch (g.state) {
        case STATE_INIT:
        case STATE_LAG:
            /* Transition to active when CO2 rises above threshold */
            if (telem->co2_ppm > CO2_ACTIVE_THRESHOLD_PPM) {
                g.active_co2_count++;
                if (g.active_co2_count >= 3) { /* 3 consecutive readings */
                    g.state = STATE_ACTIVE;
                    g.report_interval_ms = REPORT_INTERVAL_ACTIVE_MS;
                }
            } else {
                g.active_co2_count = 0;
                g.state = STATE_LAG;
                g.report_interval_ms = REPORT_INTERVAL_NORMAL_MS;
            }
            break;

        case STATE_ACTIVE:
            /* Check if approaching FG */
            if (telem->sg > 0.0f && g.target_sg > 0.0f) {
                if (fabsf(telem->sg - g.target_sg) < 0.005f) {
                    g.state = STATE_FINISHING;
                    g.report_interval_ms = REPORT_INTERVAL_NORMAL_MS;
                }
            }
            /* If CO2 drops below threshold, might be stuck */
            if (telem->co2_ppm < CO2_ACTIVE_THRESHOLD_PPM * 0.1f) {
                g.last_co2_high_ms = hal_get_ticks();
            }
            break;

        case STATE_FINISHING:
            /* Check if SG has been stable for a while (simplified) */
            /* In production, track SG readings over last 24 hours */
            break;

        case STATE_CONDITIONING:
        case STATE_COMPLETE:
            g.report_interval_ms = REPORT_INTERVAL_NORMAL_MS;
            break;

        default:
            break;
    }
}

/* ---- Send telemetry ---- */
static int send_telemetry(const bsmp_fermenter_telem_t *telem) {
    uint8_t buf[256];
    int len;

    len = bsmp_encode(BSMP_ADDR_HUB, g.seq++, BSMP_TYPE_TELEMETRY,
                      (const uint8_t *)telem, sizeof(*telem),
                      buf, sizeof(buf));
    if (len < 0) return len;

    return sx1262_send(&hal, buf, (uint8_t)len);
}

/* ---- Process incoming command ---- */
static void process_command(const bsmp_command_t *cmd) {
    switch (cmd->cmd) {
        case BSMP_CMD_SET_REPORT_INTERVAL:
            if (cmd->param_len >= 2) {
                uint16_t interval_s = ((uint16_t)cmd->params[0] << 8) | cmd->params[1];
                g.report_interval_ms = (uint32_t)interval_s * 1000UL;
            }
            break;

        case BSMP_CMD_SET_TEMP_TARGET:
            if (cmd->param_len >= 4) {
                memcpy(&g.target_temp_c, cmd->params, 4); /* float32 */
            }
            break;

        case BSMP_CMD_START_BATCH:
            g.batch_active = true;
            g.state = STATE_LAG;
            g.active_co2_count = 0;
            break;

        case BSMP_CMD_END_BATCH:
            g.batch_active = false;
            g.state = STATE_COMPLETE;
            break;

        case BSMP_CMD_CALIBRATE:
            /* Run calibration sequence */
            break;

        case BSMP_CMD_RESET:
            NVIC_SystemReset();
            break;

        default:
            break;
    }
}

/* ---- Main ---- */
int main(void) {
    bsmp_fermenter_telem_t telem;
    sx1262_rx_info_t rx_info;
    bsmp_frame_t frame;

    /* Init HAL */
    SystemInit();
    init_hal();
    hal_delay_ms(100);

    /* Init sensors */
    adxl362_init(&hal, 0, PIN_SX1262_CS);
    ds18b20_init(&hal, PIN_DS18B20);
    scd41_init(&hal, 0);
    scd41_start_periodic(&hal, 0);
    ms5837_init(&hal, 0);
    ezoph_init(&hal, 0);

    /* Init radio */
    sx1262_init(&hal, 0, PIN_SX1262_CS, PIN_SX1262_BUSY,
                PIN_SX1262_DIO1, PIN_SX1262_RST);
    sx1262_config(&rx_info, 7, 125, 5, 868000000, 14);

    /* Defaults */
    g.node_addr = 0x0001; /* Assigned during pairing in production */
    g.seq = 0;
    g.state = STATE_INIT;
    g.report_interval_ms = REPORT_INTERVAL_NORMAL_MS;
    g.sg_slope = SG_CAL_SLOPE;
    g.sg_offset = SG_CAL_OFFSET;
    g.paired = false;

    /* Main loop */
    while (1) {
        uint32_t now = hal_get_ticks();

        /* Read sensors */
        read_all_sensors(&telem);

        /* Update fermentation state */
        update_ferment_state(&telem);

        /* Send telemetry if interval elapsed */
        if ((now - g.last_report_ms) >= g.report_interval_ms) {
            send_telemetry(&telem);
            g.last_report_ms = now;
        }

        /* Check for incoming commands (non-blocking) */
        int rc = sx1262_receive(&hal, (uint8_t *)&frame, sizeof(frame),
                                &rx_info, 100);
        if (rc > 0 && rx_info.crc_ok) {
            if (frame.type == BSMP_TYPE_COMMAND && frame.len > 0) {
                bsmp_command_t cmd;
                memcpy(&cmd, frame.payload, sizeof(cmd));
                process_command(&cmd);
            }
        }

        /* Low-power sleep until next cycle */
        uint32_t sleep_ms = 1000; /* Check every second minimum */
        if (!g.batch_active || g.state == STATE_CONDITIONING) {
            sleep_ms = g.report_interval_ms - (hal_get_ticks() - g.last_report_ms);
            if (sleep_ms > 60000) sleep_ms = 60000; /* Cap at 60s */
        }
        hal_enter_lowpower(sleep_ms);
    }
}
/**
 * UrbanHarvest - Grow Pod Controller Firmware
 * ESP32-S3 + OV2640 camera + 4-channel LED + pumps + climate
 *
 * Controls the full indoor growing environment:
 * - LED spectrum and photoperiod
 * - Water pump with flow monitoring
 * - Nutrient dosing (A + B peristaltic pumps)
 * - pH adjustment dosing
 * - Climate control (fan, heater, humidifier)
 * - Camera for disease detection
 * - Sub-GHz mesh client to hub
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== CONSTANTS ========== */
#define LED_PWM_FREQ_HZ          25000   /* 25kHz PWM for LED drivers (above audible) */
#define LED_MAX_DUTY             100     /* 0-100% */
#define PUMP_FLOW_RATE_ML_S     10.0f   /* Approximate flow rate at full power */
#define NUTRIENT_STEPS_PER_ML   250     /* 28BYJ-48: ~250 half-steps per ml of solution */
#define PH_STEPS_PER_ML         400     /* 0622A micro pump: ~400 steps per ml */
#define CAMERA_WIDTH             320
#define CAMERA_HEIGHT            240
#define DISEASE_MODEL_INPUT      120    /* 120x120 input to TFLite model */
#define DISEASE_THRESHOLD        0.70f  /* Confidence threshold for alert */
#define MAX_WATER_TEMP_C         26.0f  /* Nutrient solution too warm */
#define MIN_WATER_TEMP_C         15.0f  /* Nutrient solution too cold */
#define MAX_HUMIDITY_PCT         90.0f  /* Never humidify above 90% */
#define MAX_AIR_TEMP_C           35.0f  /* Never heat above 35°C */
#define FLOW_NO_PUMP_LIMIT_S    5       /* If flow detected but pump off for 5s = leak */

/* Light schedule defaults */
#define DEFAULT_PHOTOPERIOD_ON_H   16   /* 16 hours light (vegetative) */
#define DEFAULT_PHOTOPERIOD_OFF_H  8    /* 8 hours dark */

/* ========== DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[48];
    uint16_t crc16;
} mesh_packet_t;

typedef struct {
    /* Current light schedule */
    uint8_t  light_on;
    uint8_t  red_pwm;     /* 0-100% */
    uint8_t  blue_pwm;    /* 0-100% */
    uint8_t  white_pwm;   /* 0-100% */
    uint8_t  far_red_pwm; /* 0-100% */
    uint8_t  photoperiod_on_h;
    uint8_t  photoperiod_off_h;
    uint32_t light_on_time_ms;
    uint32_t light_off_time_ms;
} light_schedule_t;

typedef struct {
    /* Nutrient dosing state */
    float nutrient_a_total_ml;
    float nutrient_b_total_ml;
    float ph_dose_total_ml;
    float nutrient_a_last_dose_ml;
    float nutrient_b_last_dose_ml;
    float ph_last_dose_ml;
    uint32_t last_dose_time_ms;
} nutrient_state_t;

typedef struct {
    /* Irrigation state */
    uint8_t  pump_running;
    uint16_t target_volume_ml;
    uint16_t delivered_volume_ml;
    uint32_t pump_start_time_ms;
    uint8_t  current_plant_id;
} irrigation_state_t;

typedef struct {
    /* Climate control setpoints */
    float target_temp_c;
    float target_humidity_pct;
    uint8_t fan_speed_pct;
    uint8_t heater_on;
    uint8_t humidifier_on;
} climate_state_t;

/* Disease detection results */
typedef struct {
    uint8_t  disease_class;     /* 0=healthy, 1=powdery_mildew, 2=downy_mildew, 3=leaf_spot_bact, 4=leaf_spot_fung, 5=nutrient_def */
    float    confidence;
    uint8_t  alert_triggered;
} disease_result_t;

/* ========== GLOBALS ========== */

static light_schedule_t light = {
    .light_on = 0,
    .red_pwm = 80,
    .blue_pwm = 70,
    .white_pwm = 60,
    .far_red_pwm = 20,
    .photoperiod_on_h = DEFAULT_PHOTOPERIOD_ON_H,
    .photoperiod_off_h = DEFAULT_PHOTOPERIOD_OFF_H,
};

static nutrient_state_t nutrients;
static irrigation_state_t irrigation;
static climate_state_t climate = {
    .target_temp_c = 24.0f,
    .target_humidity_pct = 60.0f,
    .fan_speed_pct = 30,
    .heater_on = 0,
    .humidifier_on = 0,
};

static disease_result_t disease_result;

/* Environment readings */
static float current_temp_c = 0;
static float current_humidity_pct = 0;
static float current_pressure_hpa = 0;
static float water_temp_c = 0;
static float par_umol_m2s = 0;
static uint16_t flow_pulse_count = 0;
static float flow_rate_ml_s = 0;

/* ========== LED CONTROL ========== */

/**
 * led_set_spectrum - Set all 4 LED channels
 * red, blue, white, far_red: 0-100% duty cycle
 */
static void led_set_spectrum(uint8_t red, uint8_t blue, uint8_t white, uint8_t far_red)
{
    light.red_pwm = red;
    light.blue_pwm = blue;
    light.white_pwm = white;
    light.far_red_pwm = far_red;

    /* Map 0-100% to PWM duty cycle */
    /* AL8860: PWM input frequency 25kHz, duty 0-100% */
    /* TODO: Configure PWM hardware for GPIO19,20,21,26 */

    printk("LED spectrum: R=%d%% B=%d%% W=%d%% FR=%d%%\n",
           red, blue, white, far_red);
}

/**
 * led_set_vegetative - Spectrum optimized for vegetative growth
 * High blue for compact growth, moderate red for photosynthesis
 */
static void led_set_vegetative(void)
{
    led_set_spectrum(75, 80, 50, 15);
}

/**
 * led_set_flowering - Spectrum optimized for flowering/fruiting
 * High red + far red for flower induction
 */
static void led_set_flowering(void)
{
    led_set_spectrum(90, 50, 40, 35);
}

/**
 * led_set_seedling - Spectrum optimized for seedlings/clones
 * Gentle light, high blue for root development
 */
static void led_set_seedling(void)
{
    led_set_spectrum(40, 60, 70, 5);
}

/**
 * led_on / led_off - Control photoperiod
 */
static void led_on(void)
{
    if (!light.light_on) {
        light.light_on = 1;
        light.light_on_time_ms = k_uptime_get_32();
        /* Apply current spectrum */
        led_set_spectrum(light.red_pwm, light.blue_pwm,
                         light.white_pwm, light.far_red_pwm);
        printk("Lights ON — photoperiod %dh on / %dh off\n",
               light.photoperiod_on_h, light.photoperiod_off_h);
    }
}

static void led_off(void)
{
    if (light.light_on) {
        light.light_on = 0;
        light.light_off_time_ms = k_uptime_get_32();
        /* All channels to 0 */
        led_set_spectrum(0, 0, 0, 0);
        printk("Lights OFF\n");
    }
}

/**
 * led_check_schedule - Check if it's time to toggle lights
 * Based on elapsed time since last on/off transition
 */
static void led_check_schedule(void)
{
    uint32_t now = k_uptime_get_32();
    uint32_t period_on_ms = light.photoperiod_on_h * 3600000UL;
    uint32_t period_off_ms = light.photoperiod_off_h * 3600000UL;

    if (light.light_on) {
        if ((now - light.light_on_time_ms) >= period_on_ms) {
            led_off();
        }
    } else {
        if ((now - light.light_off_time_ms) >= period_off_ms) {
            led_on();
        }
    }
}

/* ========== IRRIGATION CONTROL ========== */

/**
 * pump_start - Start irrigation for a plant
 * volume_ml: target water volume in milliliters
 */
static void pump_start(uint8_t plant_id, uint16_t volume_ml)
{
    if (irrigation.pump_running) {
        printk("Pump already running, ignoring command\n");
        return;
    }

    irrigation.pump_running = 1;
    irrigation.current_plant_id = plant_id;
    irrigation.target_volume_ml = volume_ml;
    irrigation.delivered_volume_ml = 0;
    irrigation.pump_start_time_ms = k_uptime_get_32();
    flow_pulse_count = 0;

    /* Turn on MOSFET for water pump */
    /* TODO: gpio_pin_set(gpio_dev, PUMP_MOSFET_PIN, 1); */
    printk("Pump ON: plant %d, target %d ml\n", plant_id, volume_ml);
}

/**
 * pump_stop - Stop irrigation pump
 */
static void pump_stop(void)
{
    if (!irrigation.pump_running) return;

    irrigation.pump_running = 0;
    /* TODO: gpio_pin_set(gpio_dev, PUMP_MOSFET_PIN, 0); */
    printk("Pump OFF: delivered %d ml to plant %d\n",
           irrigation.delivered_volume_ml, irrigation.current_plant_id);
}

/**
 * pump_check_flow - Monitor flow sensor and check if target volume reached
 * YF-S201: 1 pulse = 2.25ml (approximate, calibrated per unit)
 */
static void pump_check_flow(void)
{
    if (!irrigation.pump_running) return;

    /* Each flow sensor pulse ≈ 2.25ml */
    irrigation.delivered_volume_ml = flow_pulse_count * 2.25f;

    /* Check if target reached */
    if (irrigation.delivered_volume_ml >= irrigation.target_volume_ml) {
        pump_stop();
        return;
    }

    /* Safety: max pump run time (5 minutes) */
    uint32_t elapsed = k_uptime_get_32() - irrigation.pump_start_time_ms;
    if (elapsed > 300000) {
        printk("Pump timeout after 5 min — stopping\n");
        pump_stop();
        return;
    }

    /* Dry pump protection: if no flow detected after 10 seconds */
    if (elapsed > 10000 && flow_pulse_count == 0) {
        printk("No flow detected — pump may be dry, stopping\n");
        pump_stop();
        return;
    }
}

/* ========== NUTRIENT DOSING ========== */

/**
 * dose_nutrient_a - Run peristaltic pump A for a given volume
 * Uses 28BYJ-48 stepper motor via ULN2003 driver
 */
static void dose_nutrient_a(float volume_ml)
{
    uint32_t total_steps = (uint32_t)(volume_ml * NUTRIENT_STEPS_PER_ML);
    printk("Dosing nutrient A: %.1f ml (%u steps)\n", volume_ml, total_steps);

    /* TODO: Step the ULN2003 #1 for total_steps half-steps */
    /* Step sequence: IN1-IN4 pattern for 28BYJ-48 */
    /* Speed: ~500 steps/second (adjustable) */

    nutrients.nutrient_a_total_ml += volume_ml;
    nutrients.nutrient_a_last_dose_ml = volume_ml;
    nutrients.last_dose_time_ms = k_uptime_get_32();
}

/**
 * dose_nutrient_b - Run peristaltic pump B for a given volume
 */
static void dose_nutrient_b(float volume_ml)
{
    uint32_t total_steps = (uint32_t)(volume_ml * NUTRIENT_STEPS_PER_ML);
    printk("Dosing nutrient B: %.1f ml (%u steps)\n", volume_ml, total_steps);

    /* TODO: Step the ULN2003 #2 for total_steps half-steps */

    nutrients.nutrient_b_total_ml += volume_ml;
    nutrients.nutrient_b_last_dose_ml = volume_ml;
    nutrients.last_dose_time_ms = k_uptime_get_32();
}

/**
 * dose_ph_adjust - Run pH dosing pump for a given volume
 * Uses 0622A micro peristaltic pump via DRV8833
 */
static void dose_ph_adjust(float volume_ml)
{
    uint32_t total_steps = (uint32_t)(volume_ml * PH_STEPS_PER_ML);
    printk("Dosing pH adjust: %.1f ml (%u steps)\n", volume_ml, total_steps);

    /* TODO: Step the DRV8833 for total_steps */

    nutrients.ph_dose_total_ml += volume_ml;
    nutrients.ph_last_dose_ml = volume_ml;
}

/* ========== CLIMATE CONTROL ========== */

/**
 * climate_control - Adjust fan, heater, humidifier based on readings
 */
static void climate_control(void)
{
    /* Temperature control */
    if (current_temp_c > climate.target_temp_c + 2.0f) {
        /* Too hot: increase fan, turn off heater */
        climate.fan_speed_pct = 80;
        climate.heater_on = 0;
        /* TODO: Set fan PWM to 80% */
        /* TODO: Turn off heater SSR */
    } else if (current_temp_c > climate.target_temp_c + 0.5f) {
        /* Slightly warm: moderate fan */
        climate.fan_speed_pct = 50;
        climate.heater_on = 0;
    } else if (current_temp_c < climate.target_temp_c - 2.0f) {
        /* Too cold: turn on heater, reduce fan */
        climate.heater_on = 1;
        climate.fan_speed_pct = 20;
        /* TODO: Turn on heater SSR */
    } else if (current_temp_c < climate.target_temp_c - 0.5f) {
        /* Slightly cool: low fan, gentle heat */
        climate.heater_on = 1;
        climate.fan_speed_pct = 25;
    } else {
        /* In range: normal fan, no heat */
        climate.fan_speed_pct = 30;
        climate.heater_on = 0;
    }

    /* Safety: never heat above 35°C */
    if (current_temp_c >= MAX_AIR_TEMP_C) {
        climate.heater_on = 0;
        climate.fan_speed_pct = 100;
    }

    /* Humidity control */
    if (current_humidity_pct < climate.target_humidity_pct - 10.0f) {
        climate.humidifier_on = 1;
    } else if (current_humidity_pct > climate.target_humidity_pct + 5.0f) {
        climate.humidifier_on = 0;
    }

    /* Safety: never humidify above 90% RH */
    if (current_humidity_pct >= MAX_HUMIDITY_PCT) {
        climate.humidifier_on = 0;
    }

    printk("Climate: %.1fC/%.0f%%RH → fan=%d%% heat=%d humid=%d\n",
           current_temp_c, current_humidity_pct,
           climate.fan_speed_pct, climate.heater_on, climate.humidifier_on);
}

/* ========== CAMERA & DISEASE DETECTION ========== */

/**
 * camera_capture - Take a photo with OV2640
 * Captures 320×240 JPEG, downscales to 120×120 for ML model
 */
static void camera_capture(void)
{
    printk("Capturing plant photo...\n");

    /* TODO: ESP32-S3 camera driver (esp_camera library)
     * 1. Configure OV2640: JPEG mode, 320×240, quality 10
     * 2. Capture frame buffer
     * 3. Decode JPEG to RGB
     * 4. Downscale 320×240 → 120×120
     * 5. Convert to int8 for TFLite model
     * 6. Run inference
     * 7. If disease detected (confidence > threshold), send DISEASE_ALERT to hub
     * 8. Store image to SD for timelapse
     */

    /* Placeholder: simulate disease detection */
    disease_result.disease_class = 0;  /* Healthy */
    disease_result.confidence = 0.92f;
    disease_result.alert_triggered = 0;

    printk("Disease check: class=%d confidence=%.2f\n",
           disease_result.disease_class, disease_result.confidence);
}

/* ========== SUB-GHZ MESH CLIENT ========== */

/**
 * mesh_send_status - Send grow pod status to hub
 */
static void mesh_send_status(void)
{
    mesh_packet_t pkt;
    pkt.src_id = 1;  /* Grow pod node ID */
    pkt.dst_id = 0;  /* Hub */
    pkt.msg_type = 0x04;  /* GROW_POD_STATUS */
    pkt.seq_num++;

    /* Pack status into payload */
    pkt.payload[0] = irrigation.pump_running;
    /* Nutrient totals as 16-bit (ml * 10) */
    uint16_t na = (uint16_t)(nutrients.nutrient_a_total_ml * 10);
    uint16_t nb = (uint16_t)(nutrients.nutrient_b_total_ml * 10);
    uint16_t ph = (uint16_t)(nutrients.ph_dose_total_ml * 10);
    pkt.payload[1] = (na >> 8) & 0xFF;
    pkt.payload[2] = na & 0xFF;
    pkt.payload[3] = (nb >> 8) & 0xFF;
    pkt.payload[4] = nb & 0xFF;
    pkt.payload[5] = (ph >> 8) & 0xFF;
    pkt.payload[6] = ph & 0xFF;
    pkt.payload[7] = climate.fan_speed_pct;
    pkt.payload[8] = climate.heater_on;
    pkt.payload[9] = climate.humidifier_on;
    pkt.payload[10] = light.light_on;
    pkt.payload[11] = light.red_pwm;
    pkt.payload[12] = light.blue_pwm;
    pkt.payload[13] = light.white_pwm;
    pkt.payload[14] = light.far_red_pwm;
    /* Water temperature as int8 (°C, offset by 40) */
    pkt.payload[15] = (uint8_t)(water_temp_c + 40.0f);
    /* Disease alert */
    pkt.payload[16] = disease_result.disease_class;
    pkt.payload[17] = (uint8_t)(disease_result.confidence * 100);

    /* TODO: sx1261_transmit(&pkt); */
}

/* ========== MAIN LOOP ========== */

void main(void)
{
    printk("UrbanHarvest Grow Pod Controller starting...\n");

    /* Initialize peripherals */
    /* TODO: SX1261 radio init */
    /* TODO: OV2640 camera init */
    /* TODO: BME688 + TSL25911 sensor init */
    /* TODO: DS18B20 water temp init */
    /* TODO: YF-S201 flow sensor interrupt init */
    /* TODO: LED PWM channels init */
    /* TODO: ULN2003 stepper drivers init */
    /* TODO: DRV8833 pH doser init */
    /* TODO: Relays init (all off) */

    /* Start with vegetative light spectrum */
    led_set_vegetative();
    led_on();

    printk("Grow pod ready — light schedule: %dh on / %dh off\n",
           light.photoperiod_on_h, light.photoperiod_off_h);

    uint32_t last_camera_ms = 0;
    uint32_t last_status_ms = 0;
    uint32_t last_climate_ms = 0;

    while (1) {
        uint32_t now = k_uptime_get_32();

        /* Light schedule check (every 60 seconds) */
        led_check_schedule();

        /* Climate control (every 30 seconds) */
        if ((now - last_climate_ms) > 30000) {
            /* TODO: Read BME688 */
            climate_control();
            last_climate_ms = now;
        }

        /* Pump flow monitoring (every 1 second) */
        pump_check_flow();

        /* Camera capture (every 24 hours at "morning" = lights on + 2h) */
        if ((now - last_camera_ms) > 86400000) {
            camera_capture();
            last_camera_ms = now;
        }

        /* Send status to hub (every 60 seconds) */
        if ((now - last_status_ms) > 60000) {
            mesh_send_status();
            last_status_ms = now;
        }

        /* Main loop tick */
        k_msleep(1000);
    }
}
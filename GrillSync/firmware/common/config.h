/*
 * GrillSync — Configuration Header
 * Pin assignments, network parameters, thresholds
 */
#ifndef GRILLSYNC_CONFIG_H
#define GRILLSYNC_CONFIG_H

/* === Network parameters === */
#define GS_NET_FREQ_HZ       868000000  /* 868 MHz EU / 915 MHz US */
#define GS_NET_BW_HZ         125000     /* 125 kHz bandwidth */
#define GS_NET_SF            7          /* Spreading factor 7 */
#define GS_NET_CR            5          /* Coding rate 4/5 */
#define GS_NET_PREAMBLE      8
#define GS_NET_TX_POWER_DBM 22

/* === Hub GPIO assignments (ESP32-S3) === */
#define HUB_GPIO_SX_DIO1     4
#define HUB_GPIO_SX_BUSY     5
#define HUB_GPIO_SX_NSS      6
#define HUB_GPIO_SX_RST      7
#define HUB_GPIO_SX_SCK      8
#define HUB_GPIO_SX_MISO     9
#define HUB_GPIO_SX_MOSI     10
#define HUB_GPIO_BME_SDA     11
#define HUB_GPIO_BME_SCL     12
#define HUB_GPIO_RTC_SDA     13
#define HUB_GPIO_RTC_SCL     14
#define HUB_GPIO_SD_MOSI     15
#define HUB_GPIO_SD_MISO     16
#define HUB_GPIO_SD_SCK      17
#define HUB_GPIO_SD_CS       18
#define HUB_GPIO_TFT_SCK     19
#define HUB_GPIO_TFT_MOSI   20
#define HUB_GPIO_TFT_CS      21
#define HUB_GPIO_TFT_DC      35
#define HUB_GPIO_TFT_RST     36
#define HUB_GPIO_TFT_BL      37
#define HUB_GPIO_LED_RING    38
#define HUB_GPIO_GAS_RELAY   39
#define HUB_GPIO_BUZZER      40
#define HUB_GPIO_STATUS_LED  41

/* === Grill Sentinel GPIO assignments (ESP32-S3) === */
#define SENTINEL_GPIO_SX_DIO1    4
#define SENTINEL_GPIO_SX_BUSY    5
#define SENTINEL_GPIO_SX_NSS     6
#define SENTINEL_GPIO_SX_RST     7
#define SENTINEL_GPIO_SX_SCK     8
#define SENTINEL_GPIO_SX_MISO   9
#define SENTINEL_GPIO_SX_MOSI   10
#define SENTINEL_GPIO_MLX_SDA   11
#define SENTINEL_GPIO_MLX_SCL   12
#define SENTINEL_GPIO_BME_SDA   13
#define SENTINEL_GPIO_BME_SCL   14
#define SENTINEL_GPIO_MQ2_ADC   15
#define SENTINEL_GPIO_FLAME_ADC  16
#define SENTINEL_GPIO_FLAME_IRQ  17
#define SENTINEL_GPIO_PIEZO_ADC  18
#define SENTINEL_GPIO_LED       19
#define SENTINEL_GPIO_MLX_EN    20

/* === Meat Probe GPIO assignments (nRF52840) === */
#define MP_GPIO_TC1_CS      2
#define MP_GPIO_TC2_CS      3
#define MP_GPIO_TC3_CS      4
#define MP_GPIO_TC4_CS      5
#define MP_GPIO_TC_SCK      6
#define MP_GPIO_TC_MISO     7
#define MP_GPIO_LED          8
#define MP_GPIO_VBAT         9
#define MP_GPIO_USB_DETECT  10
#define MP_GPIO_BUTTON_A    11
#define MP_GPIO_BUTTON_B    12
#define MP_GPIO_CHG_STAT    13
#define MP_GPIO_PROBE_EN    14
#define MP_GPIO_BLE_IRQ     15
#define MP_GPIO_TEMP_ALERT  16

/* === Smoke Node GPIO assignments (ESP32-S3) === */
#define SMOKE_GPIO_SX_DIO1    4
#define SMOKE_GPIO_SX_BUSY    5
#define SMOKE_GPIO_SX_NSS     6
#define SMOKE_GPIO_SX_RST     7
#define SMOKE_GPIO_SX_SCK     8
#define SMOKE_GPIO_SX_MISO   9
#define SMOKE_GPIO_SX_MOSI   10
#define SMOKE_GPIO_I2C_SDA   11
#define SMOKE_GPIO_I2C_SCL   12
#define SMOKE_GPIO_MQ135_ADC  13
#define SMOKE_GPIO_PMS_TX     14
#define SMOKE_GPIO_PMS_RX     15
#define SMOKE_GPIO_FLAME_ADC  16
#define SMOKE_GPIO_FLAME_IRQ  17
#define SMOKE_GPIO_LED        18
#define SMOKE_GPIO_PMS_EN     19

/* === Grill sentinel thresholds === */
#define GS_THERMAL_FLARE_TEMP_C      400   /* Grill surface temp for fire alert */
#define GS_HOT_ZONE_TEMP_C           260   /* Zone above this = flare risk */
#define GS_HOT_ZONE_MIN_PIXELS        4    /* Min pixels for a hot zone */
#define GS_CHILD_ZONE_TEMP_LOW_C      30   /* Human body temp lower bound */
#define GS_CHILD_ZONE_TEMP_HIGH_C     37   /* Human body temp upper bound */
#define GS_CHILD_ZONE_MIN_PIXELS      4    /* Min pixels for human detection */
#define GS_GAS_LEAK_10PCT_LEL_PPM    2100   /* 10% LEL propane alarm threshold */
#define GS_GAS_LEAK_25PCT_LEL_PPM    5250   /* 25% LEL emergency threshold */
#define GS_GAS_BASELINE_WARMUP_MS   30000  /* MQ-2 baseline warmup */
#define GS_GAS_SAMPLE_RATE_HZ         10   /* Gas sensor sample rate */
#define GS_FLAME_THRESHOLD_ADC        200  /* IR flame detector threshold */
#define GS_PIEZO_FLARE_THRESHOLD      500  /* Acoustic flare-up threshold */
#define GS_THERMAL_FRAME_RATE_HZ      2    /* Thermal frame rate during cook */
#define GS_THERMAL_FRAME_RATE_IDLE   0.1   /* Thermal frame rate idle */
#define GS_FLAREUP_RISK_THRESHOLD    70   /* % risk to send flare-up warning */
#define GS_FLAREUP_ETA_MAX_MS       15000  /* Max ETA for flare-up prediction */

/* === Meat probe thresholds === */
#define GS_TC_SAMPLE_RATE_HZ          2   /* Thermocouple sample rate */
#define GS_TC_SAMPLE_RATE_IDLE_HZ     0.1  /* Idle sample rate */
#define GS_TC_OVERTEMP_C              300  /* Probe cable overtemp */
#define GS_PROBE_LOW_BATTERY_MV       3300 /* Battery low threshold */
#define GS_PROBE_CRITICAL_BATTERY_MV  3100 /* Battery critical */
#define GS_TC_MOVING_AVG_TAPS           5   /* Moving average filter taps */
#define GS_MAX_PROBES                   8   /* Max concurrent meat probes */

/* === Smoke node thresholds === */
#define GS_PM_SAMPLE_RATE_HZ           1   /* PM sensor sample rate */
#define GS_VOC_SAMPLE_RATE_HZ          1   /* VOC sensor sample rate */
#define GS_SMOKE_CREOSOTE_VOC          250  /* VOC index for creosote */
#define GS_SMOKE_DIRTY_PM25            150  /* PM2.5 threshold for dirty smoke */
#define GS_SMOKE_CLEAN_PM25             30  /* PM2.5 threshold for clean smoke */

/* === Hub thresholds === */
#define GS_GAS_SHUTOFF_DURATION_MS  60000  /* Gas shutoff minimum duration */
#define GS_BUZZER_DURATION_MS        3000  /* Buzzer ON duration */
#define GS_DISPLAY_REFRESH_MS        2000 /* TFT refresh interval */
#define GS_SD_BUFFER_DAYS               7  /* Local buffer capacity (days) */
#define GS_LED_RING_BRIGHTNESS         80  /* LED ring brightness % */

/* === Mesh parameters === */
#define GS_MESH_MAX_NODES           16
#define GS_MESH_MAX_SENTINELS        2
#define GS_MESH_MAX_SMOKE_NODES      1
#define GS_MESH_MAX_PROBES           8
#define GS_SLOT_COUNT               16
#define GS_HUB_NODE_ID              0
#define GS_TDMA_FRAME_MS            500

/* === Doneness levels === */
enum gs_doneness {
    GS_DONENESS_RAW       = 0,
    GS_DONENESS_RARE      = 1,
    GS_DONENESS_MED_RARE  = 2,
    GS_DONENESS_MEDIUM    = 3,
    GS_DONENESS_MED_WELL  = 4,
    GS_DONENESS_WELL      = 5,
};

/* === Meat types === */
enum gs_meat_type {
    GS_MEAT_BEEF      = 0,
    GS_MEAT_PORK      = 1,
    GS_MEAT_CHICKEN   = 2,
    GS_MEAT_FISH      = 3,
    GS_MEAT_LAMB      = 4,
    GS_MEAT_VEAL      = 5,
    GS_MEAT_GAME      = 6,
    GS_MEAT_CUSTOM    = 7,
    GS_MEAT_TYPE_COUNT = 8,
};

/* === Alert priorities === */
enum gs_alert_priority {
    GS_PRIORITY_LOW        = 0,
    GS_PRIORITY_MEDIUM     = 1,
    GS_PRIORITY_HIGH        = 2,
    GS_PRIORITY_CRITICAL   = 3,
};

#endif /* GRILLSYNC_CONFIG_H */
/*
 * EchoSync — Configuration Header
 * Pin assignments, network parameters, thresholds
 */
#ifndef ECHOSYNC_CONFIG_H
#define ECHOSYNC_CONFIG_H

/* === Network parameters === */
#define ES_NET_FREQ_HZ       868000000  /* 868 MHz EU / 915 MHz US */
#define ES_NET_BW_HZ         125000     /* 125 kHz bandwidth */
#define ES_NET_SF            7          /* Spreading factor 7 */
#define ES_NET_CR            5          /* Coding rate 4/5 */
#define ES_NET_PREAMBLE      8
#define ES_NET_TX_POWER_DBM  22

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
#define HUB_GPIO_EINK_SCK    19
#define HUB_GPIO_EINK_DIN    20
#define HUB_GPIO_EINK_CS     21
#define HUB_GPIO_EINK_DC     35
#define HUB_GPIO_EINK_RST    36
#define HUB_GPIO_EINK_BUSY   37
#define HUB_GPIO_LED_MATRIX  38
#define HUB_GPIO_BED_SHAKER  39
#define HUB_GPIO_BUZZER      40
#define HUB_GPIO_STATUS_LED  41

/* === Room Sentinel GPIO assignments (ESP32-S3) === */
#define SENTINEL_GPIO_SX_DIO1    4
#define SENTINEL_GPIO_SX_BUSY    5
#define SENTINEL_GPIO_SX_NSS     6
#define SENTINEL_GPIO_SX_RST     7
#define SENTINEL_GPIO_SX_SCK     8
#define SENTINEL_GPIO_SX_MISO    9
#define SENTINEL_GPIO_SX_MOSI    10
#define SENTINEL_GPIO_I2S_BCLK   11
#define SENTINEL_GPIO_I2S_LRCLK  12
#define SENTINEL_GPIO_I2S_DATA   13
#define SENTINEL_GPIO_SHT40_SDA  14
#define SENTINEL_GPIO_SHT40_SCL  15
#define SENTINEL_GPIO_LED        18
#define SENTINEL_GPIO_MIC_EN     19

/* === Wrist Band GPIO assignments (nRF52840) === */
#define WB_GPIO_OLED_SDA     2
#define WB_GPIO_OLED_SCL     3
#define WB_GPIO_HAPTIC_SDA   4
#define WB_GPIO_HAPTIC_SCL   5
#define WB_GPIO_IMU_SDA      6
#define WB_GPIO_IMU_SCL      7
#define WB_GPIO_IMU_INT      8
#define WB_GPIO_LED          9
#define WB_GPIO_VBAT         10
#define WB_GPIO_USB_DETECT   11
#define WB_GPIO_BUTTON_A     12
#define WB_GPIO_BUTTON_B     13
#define WB_GPIO_CHG_STAT     14
#define WB_GPIO_HAPTIC_EN    15
#define WB_GPIO_OLED_RST     16

/* === Door Tag GPIO assignments (nRF52840) === */
#define DT_GPIO_I2S_BCLK     2
#define DT_GPIO_I2S_LRCLK    3
#define DT_GPIO_I2S_DATA     4
#define DT_GPIO_PIEZO_ADC    5
#define DT_GPIO_LED          6
#define DT_GPIO_VBAT         7
#define DT_GPIO_BUTTON       8
#define DT_GPIO_STATUS_LED   9
#define DT_GPIO_MIC_EN       10
#define DT_GPIO_PIEZO_CMP    11

/* === Sound detection thresholds === */
#define ES_DETECTION_CONFIDENCE_MIN   70   /* % minimum confidence to report */
#define ES_DETECTION_COOLDOWN_MS      5000 /* Suppress same-class re-detection for 5s */
#define ES_EMERGENCY_COOLDOWN_MS      1000 /* Emergency sounds: 1s cooldown */
#define ES_MIC_GAIN_DB                20   /* Microphone gain */
#define ES_AUDIO_SAMPLE_RATE          16000
#define ES_AUDIO_BUFFER_SECONDS       2.0
#define ES_AUDIO_BUFFER_SAMPLES       32000

/* === Alert thresholds === */
#define ES_EMERGENCY_PRIORITY   2
#define ES_IMPORTANT_PRIORITY   1
#define ES_INFO_PRIORITY        0

/* === Wrist band settings === */
#define WB_SLEEP_SUPPRESS       1   /* Suppress non-emergency during sleep */
#define WB_BATTERY_LOW_MV       3300
#define WB_HAPTIC_INTENSITY      100  /* % haptic motor intensity */

/* === Door tag settings === */
#define DT_PIEZO_THRESHOLD       2048  /* ADC threshold for knock detection */
#define DT_KNOCK_WINDOW_MS        2000  /* Window for multi-knock detection */
#define DT_MIC_LISTEN_INTERVAL_S  30    /* Active listen interval (seconds) */
#define DT_MIC_LISTEN_DURATION_MS 2000 /* Duration of active listen */
#define DT_BATTERY_LOW_MV         2700

/* === Hub thresholds === */
#define HUB_BED_SHAKER_DURATION_MS 5000  /* Bed shaker ON duration */
#define HUB_BUZZER_DURATION_MS      3000  /* Buzzer ON duration for emergency */
#define HUB_DISPLAY_REFRESH_MS      5000  /* E-ink refresh interval */
#define HUB_SD_BUFFER_DAYS          30     /* Local buffer capacity (days) */

/* === Mesh parameters === */
#define ES_MESH_MAX_NODES       16
#define ES_MESH_MAX_SENTINELS   6
#define ES_MESH_MAX_DOOR_TAGS   8
#define ES_MESH_MAX_WRIST_BANDS 2

#endif /* ECHOSYNC_CONFIG_H */
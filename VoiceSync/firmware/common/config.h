/*
 * VoiceSync — Configuration Constants
 * Pin assignments, network parameters, calibration defaults
 */
#ifndef VOICESYNC_CONFIG_H
#define VOICESYNC_CONFIG_H

/* === Network Parameters === */
#define VS_NET_FREQ_HZ        868000000   /* 868 MHz EU / 915 MHz US */
#define VS_NET_BW_HZ          125000      /* 125 kHz bandwidth */
#define VS_NET_SF             9           /* Spreading factor 9 */
#define VS_NET_CR             0x01        /* Coding rate 4/5 */
#define VS_NET_PREAMBLE       8           /* Preamble symbols */
#define VS_NET_TX_POWER_DBM   22          /* +22 dBm (SX1262) */
#define VS_NET_AES_KEY        {0x56,0x6F,0x69,0x63,0x65,0x53,0x79,0x6E, \
                                0x63,0x4E,0x65,0x74,0x53,0x65,0x63,0x72}

/* === Hub (ESP32-S3) Pin Map === */
#define HUB_GPIO_SX_DIO1      4
#define HUB_GPIO_SX_BUSY      5
#define HUB_GPIO_SX_NSS       6
#define HUB_GPIO_SX_RST       7
#define HUB_GPIO_SX_SCK       8
#define HUB_GPIO_SX_MISO      9
#define HUB_GPIO_SX_MOSI     10
#define HUB_GPIO_BME_SDA     11
#define HUB_GPIO_BME_SCL     12
#define HUB_GPIO_RTC_SDA     13
#define HUB_GPIO_RTC_SCL     14
#define HUB_GPIO_SD_MOSI     15
#define HUB_GPIO_SD_MISO     16
#define HUB_GPIO_SD_SCK      17
#define HUB_GPIO_SD_CS       18
#define HUB_GPIO_LED         19
#define HUB_GPIO_BUZZER      20
#define HUB_GPIO_HUM_RELAY   21
#define HUB_GPIO_UART_TX     43
#define HUB_GPIO_UART_RX     44

/* === Vocal Band (nRF52840) Pin Map === */
#define VB_GPIO_I2S_SDA       2  /* P0.02 */
#define VB_GPIO_I2S_BCLK      3  /* P0.03 */
#define VB_GPIO_I2S_LRCLK     4  /* P0.04 */
#define VB_GPIO_CODEC_SDA     5  /* P0.05 */
#define VB_GPIO_CODEC_SCL     6  /* P0.06 */
#define VB_GPIO_IMU_SDA       7  /* P0.07 */
#define VB_GPIO_IMU_SCL       8  /* P0.08 */
#define VB_GPIO_TMP_SDA       9  /* P0.09 */
#define VB_GPIO_TMP_SCL      10  /* P0.10 */
#define VB_GPIO_PPG_INT      11  /* P0.11 */
#define VB_GPIO_PPG_SDA      12  /* P0.12 */
#define VB_GPIO_PPG_SCL      13  /* P0.13 */
#define VB_GPIO_LED          14  /* P0.14 */
#define VB_GPIO_VBAT         15  /* P0.15 */
#define VB_GPIO_USB_DET      16  /* P0.16 */
#define VB_GPIO_CODEC_EN     17  /* P0.17 */
#define VB_GPIO_MIC_EN       18  /* P0.18 */
#define VB_GPIO_BUTTON       19  /* P0.19 */
#define VB_GPIO_STATUS_LED   20  /* P0.20 */
#define VB_GPIO_CHG_STAT     21  /* P0.21 */

/* === Room Sentinel (ESP32-S3) Pin Map === */
#define ROOM_GPIO_SX_DIO1      4
#define ROOM_GPIO_SX_BUSY      5
#define ROOM_GPIO_SX_NSS       6
#define ROOM_GPIO_SX_RST       7
#define ROOM_GPIO_SX_SCK       8
#define ROOM_GPIO_SX_MISO      9
#define ROOM_GPIO_SX_MOSI     10
#define ROOM_GPIO_I2S_BCLK    11
#define ROOM_GPIO_I2S_LRCLK   12
#define ROOM_GPIO_I2S_DATA    13
#define ROOM_GPIO_SHT_SDA     14
#define ROOM_GPIO_SHT_SCL     15
#define ROOM_GPIO_SGP_SDA     16
#define ROOM_GPIO_SGP_SCL     17
#define ROOM_GPIO_LED         18
#define ROOM_GPIO_MIC_EN      19
#define ROOM_GPIO_UART_TX     43
#define ROOM_GPIO_UART_RX     44

/* === Hydration Tag (nRF52840) Pin Map === */
#define HYD_GPIO_HX_DOUT       2  /* P0.02 */
#define HYD_GPIO_HX_SCK        3  /* P0.03 */
#define HYD_GPIO_IMU_SDA      4  /* P0.04 */
#define HYD_GPIO_IMU_SCL       5  /* P0.05 */
#define HYD_GPIO_IMU_INT       6  /* P0.06 */
#define HYD_GPIO_LED           7  /* P0.07 */
#define HYD_GPIO_VBAT          8  /* P0.08 */
#define HYD_GPIO_BUTTON        9  /* P0.09 */
#define HYD_GPIO_STATUS_LED   10  /* P0.10 */
#define HYD_GPIO_HX_RATE      11  /* P0.11 */
#define HYD_GPIO_HX_GAIN      12  /* P0.12 */

/* === Humidity Node (ESP32) Pin Map === */
#define HUM_GPIO_SX_NSS       4
#define HUM_GPIO_SX_SCK       5
#define HUM_GPIO_SX_MISO     18
#define HUM_GPIO_SX_MOSI     23
#define HUM_GPIO_SX_DIO1     19
#define HUM_GPIO_SX_RST      21
#define HUM_GPIO_SX_BUSY     22
#define HUM_GPIO_SHT_SDA     14
#define HUM_GPIO_SHT_SCL     15
#define HUM_GPIO_HUM_RELAY   25
#define HUM_GPIO_FAN_RELAY   26
#define HUM_GPIO_US_TRIG     27
#define HUM_GPIO_US_ECHO     32
#define HUM_GPIO_LED         33
#define HUM_GPIO_BUTTON      34

/* === Sampling Intervals === */
#define VOCAL_BAND_SAMPLE_MS      5000   /* 5-second feature window */
#define VOCAL_BAND_TX_MS         30000   /* 30-second telemetry interval */
#define ROOM_SAMPLE_MS           2000   /* 2-second audio window */
#define ROOM_TX_MS              120000   /* 2-minute telemetry interval */
#define HYDRATION_TX_MS         900000   /* 15-minute telemetry interval */
#define HUMIDITY_SAMPLE_MS       60000   /* 1-minute sample */
#define HUMIDITY_TX_MS          300000   /* 5-minute telemetry */
#define HEARTBEAT_INTERVAL        3600  /* 1 hour */

/* === Voice Detection Thresholds === */
#define VOICE_NET_CONFIDENCE_PCT   75   /* VoiceNet confidence > 75% → classification */
#define PHONATION_THRESHOLD_DB    -35   /* dB threshold for voice activity */
#define PHONATION_WINDOW_MS       5000  /* 5-second phonation % window */

/* === Clinical Voice Thresholds === */
#define JITTER_NORMAL_CENTI      104   /* 1.04% × 100 */
#define JITTER_MILD_CENTI        261   /* 2.61% × 100 */
#define JITTER_MOD_CENTI         452   /* 4.52% × 100 */
#define SHIMMER_NORMAL_CENTI     381   /* 3.81% × 100 */
#define SHIMMER_MILD_CENTI       762   /* 7.62% × 100 */
#define SHIMMER_MOD_CENTI       1140   /* 11.40% × 100 */
#define HNR_NORMAL_DB             20   /* >20 dB normal */
#define HNR_MILD_DB               15   /* 15-20 dB mild */
#define HNR_MOD_DB                10   /* 10-15 dB moderate */

/* === NCVS Safe Vocal Dose Thresholds === */
#define PHONATION_SAFE_PCT        30   /* <30% of waking hours */
#define PHONATION_CONT_MIN         5   /* <5 min continuous phonation */
#define PHONATION_HOURLY_MIN      15   /* <15 min per hour */
#define VOCAL_REST_MIN            10   /* 10 min rest after excessive load */

/* === Hydration Thresholds === */
#define HYDRATION_TARGET_ML     2000   /* 2L daily target */
#define HYDRATION_LOW_PCT         60   /* <60% → dehydration alert */
#define HYDRATION_REMINDER_MIN    60   /* Reminder if no sip for 60 min */

/* === Humidity Control === */
#define HUMIDITY_TARGET_MIN       40   /* 40% RH minimum for vocal health */
#define HUMIDITY_TARGET_MAX       60   /* 60% RH maximum */
#define HUMIDITY_HYSTERESIS        3    /* ±3% hysteresis */
#define TANK_EMPTY_PCT            10   /* <10% → tank empty alert */

/* === Neck Posture Thresholds === */
#define NECK_FORWARD_DEG         15   /* >15° forward → poor posture */
#define NECK_POSTURE_SUSTAIN_S    30   /* >30 seconds sustained → alert */

/* === Risk Thresholds === */
#define VOICE_RISK_HIGH          51   /* High-risk mode activation */
#define VOICE_RISK_CRITICAL      76   /* Critical: clinical evaluation */
#define VOICE_HEALTH_POOR        40   /* <40 → poor vocal health */

/* === Battery Thresholds === */
#define BAT_LOW_MV              330   /* 3.30V → low battery alert (LiPo) */
#define BAT_CRIT_MV             300   /* 3.00V → critical (LiPo) */
#define BAT_FULL_MV             420   /* 4.20V → fully charged (LiPo) */
#define BAT_CR2032_LOW_MV       270   /* 2.70V → low (CR2032) */
#define BAT_CR2032_CRIT_MV      250   /* 2.50V → critical (CR2032) */

#endif /* VOICESYNC_CONFIG_H */
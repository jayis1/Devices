/*
 * WanderSync — Configuration Constants
 * Pin assignments, Sub-GHz parameters, TDMA slots, geofence defaults,
 * reminder schedule, calibration defaults
 */
#ifndef WANDERSYNC_CONFIG_H
#define WANDERSYNC_CONFIG_H

/* === Sub-GHz Network Parameters === */
#define WS_SUBGHZ_FREQ_HZ       868000000   /* 868 MHz EU/US ISM */
#define WS_SUBGHZ_SF             7           /* LoRa spreading factor 7 */
#define WS_SUBGHZ_BW_HZ          125000      /* 125 kHz bandwidth */
#define WS_SUBGHZ_TX_POWER_DBM   22          /* +22 dBm */
#define WS_SUBGHZ_PREAMBLE       8           /* 8 symbol preamble */
#define WS_SYNC_WORD             0x3445      /* LoRa sync word (private network) */

/* TDMA parameters */
#define WS_TDMA_SLOTS            16          /* 0=Hub, 1-14=nodes, 15=priority */
#define WS_TDMA_SLOT_MS          250         /* 250 ms per slot */
#define WS_TDMA_CYCLE_MS         (WS_TDMA_SLOTS * WS_TDMA_SLOT_MS)  /* 4000 ms */
#define WS_PRIORITY_SLOT         15          /* Emergency slot */
#define WS_MAX_NODES             31          /* 12 doors + 16 rooms + 1 band + 1 voice + 1 hub */

/* AES encryption */
#define WS_AES_KEY_LEN           16
#define WS_AES_CTR_NONCE_LEN     8

/* === Node Types === */
#define WS_NODE_HUB              0x00
#define WS_NODE_BAND             0x01
#define WS_NODE_DOOR             0x02
#define WS_NODE_ROOM             0x03
#define WS_NODE_VOICE            0x04

#define WS_HUB_NODE_ID           0x00
#define WS_BROADCAST             0xFF

/* === Care Hub (ESP32-S3) Pin Map === */
#define HUB_GPIO_BME_SDA         4
#define HUB_GPIO_BME_SCL         5
#define HUB_GPIO_RTC_SDA         6
#define HUB_GPIO_RTC_SCL         7
#define HUB_GPIO_SD_MOSI         8
#define HUB_GPIO_SD_MISO         9
#define HUB_GPIO_SD_SCK         10
#define HUB_GPIO_SD_CS          11
/* SX1262 Sub-GHz radio (SPI) */
#define HUB_GPIO_SX_MOSI        12
#define HUB_GPIO_SX_MISO        13
#define HUB_GPIO_SX_SCK         14
#define HUB_GPIO_SX_NSS         15
#define HUB_GPIO_SX_DIO1        16
#define HUB_GPIO_SX_RST         17
#define HUB_GPIO_SX_BUSY        18
#define HUB_GPIO_LED            19
#define HUB_GPIO_BUZZER         20
#define HUB_GPIO_STROBE         21
#define HUB_GPIO_CELL_TX        22
#define HUB_GPIO_CELL_RX        23
#define HUB_GPIO_CELL_PWR       24
#define HUB_GPIO_VBAT           25
#define HUB_GPIO_USB_PWR        26
#define HUB_GPIO_UART_TX        43
#define HUB_GPIO_UART_RX        44

/* === Wander Band (nRF52840) Pin Map === */
#define BAND_PIN_GPS_TX         3    /* P0.03 — UART to GPS RX */
#define BAND_PIN_GPS_RX         4    /* P0.04 — UART from GPS TX */
#define BAND_PIN_GPS_PPS        5    /* P0.05 */
#define BAND_PIN_GPS_EN         6    /* P0.06 — GPS power enable */
#define BAND_PIN_IMU_SDA        8    /* P0.08 — I²C */
#define BAND_PIN_IMU_SCL        9    /* P0.09 */
#define BAND_PIN_PPG_SDA       10    /* P0.10 — I²C (shared via TCA9548A) */
#define BAND_PIN_PPG_SCL       11    /* P0.11 */
#define BAND_PIN_SX_MOSI       12    /* P0.12 — SPI */
#define BAND_PIN_SX_MISO       13    /* P0.13 */
#define BAND_PIN_SX_SCK        14    /* P0.14 */
#define BAND_PIN_SX_NSS        15    /* P0.15 */
#define BAND_PIN_SX_DIO1       16    /* P0.16 */
#define BAND_PIN_SX_RST        17    /* P0.17 */
#define BAND_PIN_SX_BUSY       18    /* P0.18 */
#define BAND_PIN_HAPTIC_SDA    19    /* P0.19 — I²C (separate bus) */
#define BAND_PIN_HAPTIC_SCL    20    /* P0.20 */
#define BAND_PIN_SOS           21    /* P0.21 — GPIO input */
#define BAND_PIN_LED           22    /* P0.22 — SK6812 */
#define BAND_PIN_TAMPER        23    /* P0.23 — Band removal */
#define BAND_PIN_VBAT          24    /* P0.24 — ADC */
#define BAND_PIN_USB_PWR       25    /* P0.25 */
#define BAND_PIN_GPS_FIX_LED   26    /* P0.26 */
#define BAND_PIN_CHG_STAT      27    /* P0.27 — MCP73831 charge status */

/* === Door Sentinel (ESP32-C3) Pin Map === */
#define DOOR_GPIO_REED          2    /* Door/window open/close */
#define DOOR_GPIO_LOCK_A        3    /* H-bridge IN1 (lock) */
#define DOOR_GPIO_LOCK_B        4    /* H-bridge IN2 (unlock) */
#define DOOR_GPIO_LOCK_FB       5    /* Deadbolt position reed switch */
#define DOOR_GPIO_TAMPER        6    /* Tamper microswitch */
/* SX1262 Sub-GHz radio (SPI) */
#define DOOR_GPIO_SX_MOSI       7
#define DOOR_GPIO_SX_MISO       8
#define DOOR_GPIO_SX_SCK        9
#define DOOR_GPIO_SX_NSS       10
#define DOOR_GPIO_SX_DIO1      11
#define DOOR_GPIO_SX_RST       12
#define DOOR_GPIO_SX_BUSY      13
#define DOOR_GPIO_LED          14
#define DOOR_GPIO_BUZZER       15
#define DOOR_GPIO_VBAT         16
#define DOOR_GPIO_UART_TX      18
#define DOOR_GPIO_UART_RX      19

/* === Room Sentinel (ESP32-S3) Pin Map === */
#define ROOM_GPIO_RADAR_TX      4    /* UART2 RX ← HLK-LD2410 TX */
#define ROOM_GPIO_RADAR_RX      5    /* UART2 TX → HLK-LD2410 RX */
#define ROOM_GPIO_PIR           6    /* AM612 digital output */
/* SX1262 Sub-GHz radio (SPI) */
#define ROOM_GPIO_SX_MOSI       7
#define ROOM_GPIO_SX_MISO       8
#define ROOM_GPIO_SX_SCK        9
#define ROOM_GPIO_SX_NSS       10
#define ROOM_GPIO_SX_DIO1      11
#define ROOM_GPIO_SX_RST       12
#define ROOM_GPIO_SX_BUSY      13
#define ROOM_GPIO_LED          14
#define ROOM_GPIO_VBAT         15
#define ROOM_GPIO_USB_PWR      16
#define ROOM_GPIO_UART_TX      43
#define ROOM_GPIO_UART_RX      44

/* === Voice Node (ESP32-S3) Pin Map === */
#define VOICE_GPIO_MIC_WS       4    /* INMP441 I²S word select */
#define VOICE_GPIO_MIC_SCK      5    /* INMP441 I²S bit clock */
#define VOICE_GPIO_MIC_SD       6    /* INMP441 I²S data in */
#define VOICE_GPIO_AMP_BCLK     7    /* MAX98357A I²S bit clock */
#define VOICE_GPIO_AMP_LRCLK    8    /* MAX98357A I²S word select */
#define VOICE_GPIO_AMP_DATA     9    /* MAX98357A I²S data out */
#define VOICE_GPIO_FLASH_CS    10    /* W25Q128 SPI flash CS */
#define VOICE_GPIO_FLASH_SCK   11
#define VOICE_GPIO_FLASH_MOSI  12
#define VOICE_GPIO_FLASH_MISO  13
/* SX1262 Sub-GHz radio (SPI) */
#define VOICE_GPIO_SX_MOSI     14
#define VOICE_GPIO_SX_MISO     15
#define VOICE_GPIO_SX_SCK      16
#define VOICE_GPIO_SX_NSS      17
#define VOICE_GPIO_SX_DIO1     18
#define VOICE_GPIO_SX_RST      19
#define VOICE_GPIO_SX_BUSY     20
#define VOICE_GPIO_LED         21
#define VOICE_GPIO_VBAT        22
#define VOICE_GPIO_USB_PWR     23
#define VOICE_GPIO_UART_TX     43
#define VOICE_GPIO_UART_RX     44

/* === Sampling Intervals === */
#define BAND_GPS_OUTDOOR_MS     1000    /* 1 Hz GPS outdoors */
#define BAND_GPS_INDOOR_MS      10000   /* 0.1 Hz GPS indoors (power save) */
#define BAND_IMU_HZ             50      /* 50 Hz IMU */
#define BAND_PPG_INTERVAL_MS    900000  /* 15 min PPG (30 sec window) */
#define BAND_PPG_SLEEP_MS       30000   /* 30 sec PPG during sleep */
#define BAND_WANDERNET_MS       300000  /* 5 min WanderNet inference */
#define BAND_TELEM_MS           300000  /* 5 min telemetry to Hub */
#define BAND_SOS_DEBOUNCE_MS    2000    /* 2 sec SOS debounce */

#define DOOR_REED_POLL_MS       100     /* 10 Hz reed switch poll */
#define DOOR_TELEM_MS           60000   /* 1 min telemetry */
#define DOOR_PROXIMITY_CHECK_MS 5000    /* 5 sec band proximity check */

#define ROOM_RADAR_INTERVAL_MS  100     /* 10 Hz mmWave data */
#define ROOM_PIR_INTERVAL_MS    500     /* 2 Hz PIR */
#define ROOM_ADLNET_MS          10000   /* 10 sec ADLNet inference window */
#define ROOM_TELEM_MS           30000   /* 30 sec telemetry */

#define VOICE_KEYWORD_MS        500     /* 2 Hz keyword detection check */
#define VOICE_TELEM_MS          60000   /* 1 min telemetry */
#define HEARTBEAT_INTERVAL_S    60      /* 1 minute */

/* === Geofence Defaults === */
#define WS_GEOFENCE_RADIUS_M        200     /* Default 200 m radius */
#define WS_GEOFENCE_NIGHT_RADIUS_M  100     /* Night: 100 m (stricter) */
#define WS_GEOFENCE_NIGHT_START_H   22      /* 10 PM */
#define WS_GEOFENCE_NIGHT_END_H     6       /* 6 AM */
#define WS_GEOFENCE_NEAR_M          50      /* "Near boundary" distance */
#define WS_GEOFENCE_OUTSIDE_TIMEOUT_S 900   /* 15 min outside → 911 dispatch */

/* === Door Lock Schedule Defaults === */
#define WS_DOOR_LOCK_START_H    22      /* 10 PM lock */
#define WS_DOOR_UNLOCK_END_H    6       /* 6 AM unlock */
#define WS_DOOR_PROXIMITY_M     5       /* Lock when band <5 m */
#define WS_DOOR_LOCK_TIMEOUT_MS 5000    /* 5 sec to lock after proximity */

/* === WanderNet Risk Thresholds === */
#define WS_WANDER_RISK_LOW       30
#define WS_WANDER_RISK_MODERATE  50
#define WS_WANDER_RISK_HIGH      70
#define WS_WANDER_RISK_CRITICAL  85

/* === Fall Detection === */
#define WS_FALL_IMPACT_G         30     /* >3g impact (×0.1g) */
#define WS_FALL_STILLNESS_MS     30000  /* 30 sec stillness after impact */
#define WS_FALL_CANCEL_WINDOW_S  30     /* 30 sec SOS cancel window */

/* === SOS Escalation === */
#define WS_SOS_CAREGIVER_TIMEOUT_S  180  /* 3 min → call emergency contact */
#define WS_SOS_DISPATCH_TIMEOUT_S   300  /* 5 min → 911 dispatch */
#define WS_WANDER_CAREGIVER_TIMEOUT_S 300 /* 5 min → emergency contact */
#define WS_WANDER_DISPATCH_TIMEOUT_S  900 /* 15 min → 911 dispatch */

/* === Emergency Dispatch === */
#define WS_EMERGENCY_911_ENABLED     1
#define WS_EMERGENCY_CANCEL_WINDOW_S 60
#define WS_EMERGENCY_CONTACT_MAX     5

/* === ADLNet Classes === */
#define WS_ADL_ABSENT    0
#define WS_ADL_WALKING   1
#define WS_ADL_SITTING   2
#define WS_ADL_LYING     3
#define WS_ADL_EATING    4
#define WS_ADL_COOKING   5
#define WS_ADL_PACING    6
#define WS_ADL_STANDING  7

/* === Wander Band Activity Classes === */
#define WS_ACT_SITTING   0
#define WS_ACT_WALKING   1
#define WS_ACT_LYING     2
#define WS_ACT_STANDING  3
#define WS_ACT_FIDGETING 4
#define WS_ACT_FALL      5

/* === Alert Types === */
#define WS_ALERT_WANDER      1
#define WS_ALERT_FALL        2
#define WS_ALERT_SOS         3
#define WS_ALERT_BAND_REMOVED 4

/* === Alert Severity === */
#define WS_SEV_INFO       0
#define WS_SEV_WARNING    1
#define WS_SEV_CRITICAL   2
#define WS_SEV_EMERGENCY  3

/* === Battery Thresholds (×0.01V) === */
#define BAT_LOW_MV         330   /* 3.30V → low battery */
#define BAT_CRIT_MV        300   /* 3.00V → critical */
#define BAT_FULL_MV        420   /* 4.20V → full (LiPo) */
#define BAT_CR123A_LOW_MV  240   /* 2.40V per cell → low (2× CR123A = 4.8V → 2.4V after divider) */

/* === Reminder Types === */
#define WS_REMINDER_MEDICATION  0
#define WS_REMINDER_MEAL        1
#define WS_REMINDER_HYDRATION   2
#define WS_REMINDER_APPOINTMENT 3
#define WS_REMINDER_ORIENTATION 4
#define WS_REMINDER_CUSTOM      5

/* === Voice Clip Library === */
#define WS_CLIP_MEDICATION_START  0
#define WS_CLIP_MEDICATION_END   20
#define WS_CLIP_MEAL_START       20
#define WS_CLIP_MEAL_END         40
#define WS_CLIP_HYDRATION_START  40
#define WS_CLIP_HYDRATION_END    50
#define WS_CLIP_APPOINTMENT_START 50
#define WS_CLIP_APPOINTMENT_END  60
#define WS_CLIP_ORIENTATION_START 60
#define WS_CLIP_ORIENTATION_END  70
#define WS_CLIP_SAFETY_START     70
#define WS_CLIP_SAFETY_END       80
#define WS_CLIP_SOCIAL_START     80
#define WS_CLIP_SOCIAL_END       90
#define WS_CLIP_COMFORT_START    90
#define WS_CLIP_COMFORT_END     100
#define WS_CLIP_TIME_START      100
#define WS_CLIP_TIME_END        110
#define WS_CLIP_EMERGENCY_START 110
#define WS_CLIP_EMERGENCY_END   120
#define WS_CLIP_MAX             120

/* === Keyword Detection (Voice Node) === */
#define WS_KW_NONE      0xFF
#define WS_KW_TIME      0
#define WS_KW_HELP      1
#define WS_KW_YES       2
#define WS_KW_NO        3
#define WS_KW_WHERE     4
#define WS_KW_MEDICINE  5
#define WS_KW_FOOD      6
#define WS_KW_WATER     7
#define WS_KW_HOME      8
#define WS_KW_STOP      9

#endif /* WANDERSYNC_CONFIG_H */
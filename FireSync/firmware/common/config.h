/*
 * FireSync — Configuration Constants
 * Pin assignments, Sub-GHz parameters, TDMA slots, fire thresholds
 */
#ifndef FIRESYNC_CONFIG_H
#define FIRESYNC_CONFIG_H

/* === Sub-GHz Network Parameters === */
#define FS_SUBGHZ_FREQ_HZ       868000000   /* 868 MHz EU/US ISM */
#define FS_SUBGHZ_SF             7           /* LoRa spreading factor 7 */
#define FS_SUBGHZ_BW_HZ          125000      /* 125 kHz bandwidth */
#define FS_SUBGHZ_TX_POWER_DBM   22          /* +22 dBm */
#define FS_SUBGHZ_PREAMBLE       8           /* 8 symbol preamble */
#define FS_SYNC_WORD             0x3445      /* LoRa sync word (private network) */

/* TDMA parameters */
#define FS_TDMA_SLOTS            17          /* 0=Hub, 1-14=nodes, 15=priority, 16=reserved */
#define FS_TDMA_SLOT_MS          250         /* 250 ms per slot */
#define FS_TDMA_CYCLE_MS         (FS_TDMA_SLOTS * FS_TDMA_SLOT_MS)  /* 4250 ms */
#define FS_PRIORITY_SLOT         15          /* Emergency slot for FIRE_ALERT */
#define FS_MAX_NODES             20

/* AES encryption */
#define FS_AES_KEY_LEN           16
#define FS_AES_CTR_NONCE_LEN     8

/* === BLE (Hub Wi-Fi config fallback) === */
/* Hub uses Wi-Fi for cloud, not BLE for node comms */

/* === Node Types === */
#define FS_NODE_HUB              0x00
#define FS_NODE_SENTINEL         0x01
#define FS_NODE_STOVE            0x02
#define FS_NODE_PANEL            0x03
#define FS_NODE_ESCAPE           0x04

#define FS_HUB_NODE_ID           0x00
#define FS_BROADCAST             0xFF

/* === Hub (ESP32-S3) Pin Map === */
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

/* === Room Sentinel (ESP32-S3) Pin Map === */
/* Smoke sensor (PMSA003) + Thermal array (MLX90640) on I²C */
#define SENT_GPIO_SMOKE_SDA     4
#define SENT_GPIO_SMOKE_SCL     5
#define SENT_GPIO_THERMAL_SDA   6
#define SENT_GPIO_THERMAL_SCL   7
/* SX1262 Sub-GHz radio (SPI) */
#define SENT_GPIO_SX_MOSI       8
#define SENT_GPIO_SX_MISO       9
#define SENT_GPIO_SX_SCK       10
#define SENT_GPIO_SX_NSS       11
#define SENT_GPIO_SX_DIO1      12
#define SENT_GPIO_SX_RST       13
#define SENT_GPIO_SX_BUSY      14
/* DS18B20 temperature (1-Wire) */
#define SENT_GPIO_DS18B20      15
/* ZE07-CO sensor (UART2) */
#define SENT_GPIO_CO_TX        16   /* ESP RX ← CO TX */
#define SENT_GPIO_CO_RX        17   /* ESP TX → CO RX */
/* PIR occupancy sensor */
#define SENT_GPIO_PIR          18
#define SENT_GPIO_LED          19
#define SENT_GPIO_BUZZER       20
#define SENT_GPIO_STROBE       21
#define SENT_GPIO_VBAT         22
#define SENT_GPIO_USB_PWR      23
#define SENT_GPIO_UART_TX      43
#define SENT_GPIO_UART_RX      44

/* === Stove Guard (ESP32-S3) Pin Map === */
/* TCA9548A knob encoder mux + MLX90640 thermal (I²C) */
#define STOVE_GPIO_KNOB_SDA    4
#define STOVE_GPIO_KNOB_SCL    5
#define STOVE_GPIO_THERMAL_SDA 6
#define STOVE_GPIO_THERMAL_SCL 7
/* SX1262 Sub-GHz radio (SPI) */
#define STOVE_GPIO_SX_MOSI     8
#define STOVE_GPIO_SX_MISO     9
#define STOVE_GPIO_SX_SCK     10
#define STOVE_GPIO_SX_NSS     11
#define STOVE_GPIO_SX_DIO1    12
#define STOVE_GPIO_SX_RST     13
#define STOVE_GPIO_SX_BUSY    14
/* Gas valve L298N H-bridge */
#define STOVE_GPIO_VALVE_OPEN  15
#define STOVE_GPIO_VALVE_CLOSE 16
#define STOVE_GPIO_VALVE_FB   17   /* Reed switch: valve closed feedback */
#define STOVE_GPIO_LED        18
#define STOVE_GPIO_BUZZER     19
#define STOVE_GPIO_VBAT       20
#define STOVE_GPIO_USB_PWR    21
#define STOVE_GPIO_UART_TX    43
#define STOVE_GPIO_UART_RX    44

/* === Panel Monitor (STM32G431) Pin Map === */
#define PANEL_PIN_CT_L1        0    /* PA0 / ADC1_IN1 */
#define PANEL_PIN_CT_L2        1    /* PA1 / ADC1_IN2 */
#define PANEL_PIN_AC_VOLT      2    /* PA2 / ADC1_IN3 */
#define PANEL_PIN_TEMP_BUS     4    /* PA4 / DS18B20 #1 (1-Wire) */
#define PANEL_PIN_TEMP_NEUT    5    /* PA5 / DS18B20 #2 */
#define PANEL_PIN_TEMP_BRK1    6    /* PA6 / DS18B20 #3 */
#define PANEL_PIN_TEMP_BRK2    7    /* PA7 / DS18B20 #4 */
#define PANEL_PIN_SX_MOSI      8    /* PB0 */
#define PANEL_PIN_SX_MISO      9    /* PB1 */
#define PANEL_PIN_SX_SCK      10    /* PB2 */
#define PANEL_PIN_SX_NSS      11    /* PB3 */
#define PANEL_PIN_SX_DIO1     12    /* PB4 */
#define PANEL_PIN_SX_RST      13    /* PB5 */
#define PANEL_PIN_SX_BUSY     14    /* PB6 */
#define PANEL_PIN_SHUNT_TRIP  15    /* PB7 — triggers shunt-trip breaker */
#define PANEL_PIN_LED         16    /* PB8 */
#define PANEL_PIN_VBAT        17    /* PB9 / ADC2 */
#define PANEL_PIN_UART_TX     18    /* PA9 */
#define PANEL_PIN_UART_RX     19    /* PA10 */

/* === Escape Controller (ESP32-S3) Pin Map === */
#define ESC_GPIO_LED_FRONT     4   /* WS2812B strip 1 (front path) */
#define ESC_GPIO_LED_BACK      5   /* WS2812B strip 2 (back path) */
#define ESC_GPIO_LED_HALL      6   /* WS2812B strip 3 (hallway) */
#define ESC_GPIO_LED_STAIR     7   /* WS2812B strip 4 (stairwell) */
/* I²S audio amp (MAX98357A) */
#define ESC_GPIO_AMP_BCLK      8
#define ESC_GPIO_AMP_LRCLK     9
#define ESC_GPIO_AMP_DATA     10
/* W25Q128 SPI flash (voice clips) */
#define ESC_GPIO_FLASH_CS     11
#define ESC_GPIO_FLASH_SCK    12
#define ESC_GPIO_FLASH_MOSI   13
#define ESC_GPIO_FLASH_MISO   14
/* SX1262 Sub-GHz radio (SPI) */
#define ESC_GPIO_SX_MOSI      15
#define ESC_GPIO_SX_MISO      16
#define ESC_GPIO_SX_SCK       17
#define ESC_GPIO_SX_NSS       18
#define ESC_GPIO_SX_DIO1      19
#define ESC_GPIO_SX_RST       20
#define ESC_GPIO_SX_BUSY      21
/* Door release relays */
#define ESC_GPIO_DOOR_FRONT   22
#define ESC_GPIO_DOOR_BACK    23
#define ESC_GPIO_DOOR_GARAGE  24
#define ESC_GPIO_DOOR_BEDROOM 25
#define ESC_GPIO_VBAT         26
#define ESC_GPIO_USB_PWR      27
#define ESC_GPIO_LED          28
#define ESC_GPIO_UART_TX      43
#define ESC_GPIO_UART_RX      44

/* === Sampling Intervals === */
#define SENT_SMOKE_INTERVAL_MS   500   /* 2 Hz smoke (PMSA003) */
#define SENT_CO_INTERVAL_MS      1000  /* 1 Hz CO (ZE07) */
#define SENT_TEMP_INTERVAL_MS    2000  /* 0.5 Hz DS18B20 */
#define SENT_THERMAL_INTERVAL_MS 100   /* 10 Hz MLX90640 (16 Hz max) */
#define SENT_FLAMENET_INTERVAL_MS 500  /* 2 Hz FlameNet inference */
#define SENT_PIR_INTERVAL_MS      5000 /* 0.2 Hz PIR occupancy */
#define SENT_TELEM_INTERVAL_MS    5000 /* 5 sec telemetry to Hub */

#define STOVE_THERMAL_INTERVAL_MS 100  /* 10 Hz thermal array */
#define STOVE_KNOB_INTERVAL_MS    500  /* 2 Hz knob read */
#define STOVE_PANTEMP_INTERVAL_MS 1000 /* 1 Hz PanTemp CNN */
#define STOVE_TELEM_INTERVAL_MS   5000 /* 5 sec telemetry */

#define PANEL_ADC_SAMPLE_HZ      8000   /* 8 kHz current sampling */
#define PANEL_FFT_INTERVAL_MS    1000  /* 1 Hz FFT + ArcDetect */
#define PANEL_TEMP_INTERVAL_MS   2000  /* 0.5 Hz thermal sensors */
#define PANEL_TELEM_INTERVAL_MS   5000 /* 5 sec telemetry */

#define ESC_TELEM_INTERVAL_MS    5000  /* 5 sec telemetry */
#define HEARTBEAT_INTERVAL_S      60    /* 1 minute */

/* === Fire Detection Thresholds === */
/* Smoke (PM2.5 μg/m³) */
#define FS_SMOKE_NORMAL_UGM3      35    /* Normal indoor PM2.5 */
#define FS_SMOKE_WARN_UGM3        150   /* Warning: possible smoke */
#define FS_SMOKE_FIRE_UGM3        500   /* Fire: high smoke density */

/* CO (ppm) */
#define FS_CO_NORMAL_PPM          5     /* Normal background */
#define FS_CO_WARN_PPM            35    /* OSHA 8-hour limit */
#define FS_CO_DANGER_PPM         100    /* Immediate danger */
#define FS_CO_CRITICAL_PPM       400    /* Life-threatening */

/* Temperature rate-of-rise (°C/min, UL 217) */
#define FS_TEMP_ROR_FIRE_C        8.3   /* >8.3°C/min = fire (UL 217) */
#define FS_TEMP_ROR_WARN_C        4.0   /* >4°C/min = warning */

/* Thermal array (MLX90640) */
#define FS_THERMAL_NORMAL_C       35    /* Normal ambient max */
#define FS_THERMAL_WARN_C         80    /* Possible fire/overheat */
#define FS_THERMAL_FIRE_C        150    /* Fire: high thermal anomaly */
#define FS_THERMAL_ANOMALY_SCORE  128    /* ThermalAnomaly >128 = anomaly */

/* FlameNet */
#define FS_FLAMENET_CONF_FIRE_PCT 75    /* >75% confidence → FIRE_ALERT */
#define FS_FLAMENET_CONF_CONFIRM 85    /* >85% → immediate confirmation */

/* Stove Guard */
#define FS_PAN_TEMP_OIL_SMOKING_C 250   /* Oil smoke point */
#define FS_PAN_TEMP_OIL_IGNITE_C  340   /* Oil auto-ignition */
#define FS_STOVE_UNATTENDED_S     1800  /* 30 min unattended → shutoff */
#define FS_STOVE_NO_CHANGE_S      900   /* 15 min no heat change → warning */

/* Panel Monitor */
#define FS_BUS_BAR_WARN_C         75    /* Bus bar warning temp */
#define FS_BUS_BAR_TRIP_C         90    /* Bus bar shunt-trip temp */
#define FS_BREAKER_WARN_C         70    /* Breaker warning temp */
#define FS_BREAKER_TRIP_C         85    /* Breaker shunt-trip temp */
#define FS_ARC_CONFIDENCE_PCT     70    /* ArcDetect >70% → shunt trip */

/* === FlameNet Classes === */
#define FS_CLASS_NORMAL           0
#define FS_CLASS_COOKING          1
#define FS_CLASS_STEAM            2
#define FS_CLASS_CIGARETTE         3
#define FS_CLASS_CANDLE           4
#define FS_CLASS_SMOLDERING        5
#define FS_CLASS_FLAMING_FIRE      6

/* === ArcDetect Classes === */
#define FS_ARC_NORMAL             0
#define FS_ARC_SERIES              1
#define FS_ARC_PARALLEL            2
#define FS_ARC_OVERLOAD            3

/* === PanTemp Classes (Stove Guard) === */
#define FS_PAN_SAFE_COOKING        0
#define FS_PAN_OVERHEATING          1
#define FS_PAN_OIL_SMOKING          2
#define FS_PAN_FLAMING              3

/* === Battery Thresholds (×0.01V) === */
#define BAT_LOW_MV                 330   /* 3.30V → low battery */
#define BAT_CRIT_MV                300   /* 3.00V → critical */
#define BAT_FULL_MV                420   /* 4.20V → full (LiPo) */
#define BAT_LIFEPO4_LOW_MV         320   /* 3.20V → low (LiFePO4) */
#define BAT_LIFEPO4_FULL_MV        365   /* 3.65V → full (LiFePO4) */

/* === Emergency Dispatch === */
#define EMERGENCY_911_ENABLED      1
#define EMERGENCY_CANCEL_WINDOW_S  60
#define EMERGENCY_CONTACT_MAX      5

/* === Fire Consensus === */
#define FS_CONSENSUS_SINGLE_HIGH_PCT  85  /* Single node >85% → confirm */
#define FS_CONSENSUS_WAIT_MS       5000  /* 75-85%: wait 5s for corroboration */
#define FS_CONSENSUS_TWO_NODES          2  /* Two nodes → confirm */

/* === Suppression === */
#define FS_SUPPRESS_STOVE         1     /* Close gas valve on fire */
#define FS_SUPPRESS_HVAC          1     /* HVAC shutoff on fire */
#define FS_SUPPRESS_PANEL         1     /* Shunt trip on electrical fire */
#define FS_SUPPRESS_HOOD          0     /* Kitchen hood suppression relay (optional) */

#endif /* FIRESYNC_CONFIG_H */
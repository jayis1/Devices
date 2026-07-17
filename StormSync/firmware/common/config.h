/*
 * StormSync — Configuration Constants
 * Pin assignments, network parameters, calibration defaults
 */
#ifndef STORMSYNC_CONFIG_H
#define STORMSYNC_CONFIG_H

/* === Network Parameters === */
#define SS_NET_FREQ_HZ        868000000   /* 868 MHz EU / 915 MHz US */
#define SS_NET_BW_HZ          125000      /* 125 kHz bandwidth */
#define SS_NET_SF             9           /* Spreading factor 9 */
#define SS_NET_CR             0x01        /* Coding rate 4/5 */
#define SS_NET_PREAMBLE       8           /* Preamble symbols */
#define SS_NET_TX_POWER_DBM   22          /* +22 dBm (SX1262) */
#define SS_NET_AES_KEY        {0x53,0x74,0x6F,0x72,0x6D,0x53,0x79,0x6E, \
                                0x63,0x4E,0x65,0x74,0x77,0x72,0x6B,0x21}

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
#define HUB_GPIO_CELL_TX     21
#define HUB_GPIO_CELL_RX     22
#define HUB_GPIO_CELL_PWR    23
#define HUB_GPIO_UART_TX     43
#define HUB_GPIO_UART_RX     44

/* === Sump Sentinel (ESP32) Pin Map === */
#define SUMP_GPIO_SX_NSS       4
#define SUMP_GPIO_SX_SCK       5
#define SUMP_GPIO_SX_MISO     18
#define SUMP_GPIO_SX_MOSI     23
#define SUMP_GPIO_SX_DIO1     19
#define SUMP_GPIO_SX_RST      22
#define SUMP_GPIO_SX_BUSY     21
#define SUMP_GPIO_US_TRIG     25
#define SUMP_GPIO_US_ECHO     26
#define SUMP_GPIO_CT_CLAMP    27  /* ADC1_CH0 */
#define SUMP_GPIO_ADXL_CS     14
#define SUMP_GPIO_ADXL_SCK    12
#define SUMP_GPIO_ADXL_MISO   13
#define SUMP_GPIO_ADXL_MOSI   15
#define SUMP_GPIO_ADXL_INT1   16
#define SUMP_GPIO_FLOW        17
#define SUMP_GPIO_DS18B20     32
#define SUMP_GPIO_BAT_V       33  /* ADC1_CH5 */
#define SUMP_GPIO_MAINS       34  /* Input only */
#define SUMP_GPIO_PUMP_LED    35

/* === Saturation Probe (nRF52840) Pin Map === */
#define SOIL_GPIO_FDC_SCL     2  /* P0.02 */
#define SOIL_GPIO_FDC_SDA     3  /* P0.03 */
#define SOIL_GPIO_DS18_1      4  /* P0.04 — 15cm */
#define SOIL_GPIO_DS18_2      5  /* P0.05 — 45cm */
#define SOIL_GPIO_DS18_3      6  /* P0.06 — 90cm */
#define SOIL_GPIO_PORE_ADC    7  /* P0.07 / AIN7 */
#define SOIL_GPIO_FDC_INT     8  /* P0.08 */
#define SOIL_GPIO_SX_NSS     11  /* P0.11 */
#define SOIL_GPIO_SX_SCK     12  /* P0.12 */
#define SOIL_GPIO_SX_MISO    13  /* P0.13 */
#define SOIL_GPIO_SX_MOSI    14  /* P0.14 */
#define SOIL_GPIO_SX_DIO1    15  /* P0.15 */
#define SOIL_GPIO_SX_RST     16  /* P0.16 */
#define SOIL_GPIO_SX_BUSY    17  /* P0.17 */
#define SOIL_GPIO_VBAT       18  /* P0.18 / AIN18 */
#define SOIL_GPIO_VSOL       19  /* P0.19 / AIN19 */
#define SOIL_GPIO_LED        20  /* P0.20 */
#define SOIL_GPIO_SENSOR_SW  21  /* P0.21 */
#define SOIL_GPIO_VDDH_EN    22  /* P0.22 */

/* === Weather Sentinel (ESP32-S3) Pin Map === */
#define WX_GPIO_BME_SDA      4
#define WX_GPIO_BME_SCL      5
#define WX_GPIO_WIND_SPD     8
#define WX_GPIO_WIND_DIR     9
#define WX_GPIO_RAIN_TIP    10
#define WX_GPIO_SX_NSS      12
#define WX_GPIO_SX_SCK      13
#define WX_GPIO_SX_MISO     14
#define WX_GPIO_SX_MOSI     15
#define WX_GPIO_SX_DIO1     16
#define WX_GPIO_SX_RST      17
#define WX_GPIO_SX_BUSY     18
#define WX_GPIO_VBAT        19
#define WX_GPIO_LED         20

/* === Flood Actuator (ESP32) Pin Map === */
#define ACT_GPIO_SX_NSS       4
#define ACT_GPIO_SX_SCK       5
#define ACT_GPIO_SX_MISO     18
#define ACT_GPIO_SX_MOSI     23
#define ACT_GPIO_SX_DIO1     19
#define ACT_GPIO_SX_RST      22
#define ACT_GPIO_SX_BUSY     21
#define ACT_GPIO_VALVE_CLOSE 25
#define ACT_GPIO_VALVE_OPEN  26
#define ACT_GPIO_PUMP_RELAY  27
#define ACT_GPIO_FLOAT_SW    14
#define ACT_GPIO_SIREN       12
#define ACT_GPIO_MAINS       13
#define ACT_GPIO_BAT_V       32  /* ADC1_CH4 */
#define ACT_GPIO_VALVE_CL    33  /* Reed: closed position */
#define ACT_GPIO_VALVE_OP    34  /* Input only — Reed: open position */
#define ACT_GPIO_OVERRIDE    35  /* Input only — manual override button */

/* === Sampling Intervals (seconds) === */
#define SUMP_SAMPLE_INTERVAL      30    /* 30 seconds (normal) */
#define SUMP_SAMPLE_INTERVAL_STORM 15   /* 15 seconds (storm mode) */
#define SOIL_SAMPLE_INTERVAL      900   /* 15 minutes */
#define WEATHER_SAMPLE_INTERVAL   300   /* 5 minutes */
#define HEARTBEAT_INTERVAL        3600  /* 1 hour */

/* === Sump Pit Safety Thresholds === */
#define SUMP_PIT_DEPTH_CM        120    /* Typical sump pit depth */
#define SUMP_WARN_LEVEL_PCT      70     /* 70% → Warning */
#define SUMP_CRITICAL_LEVEL_PCT  85     /* 85% → Critical */
#define SUMP_EMERGENCY_PCT       95     /* 95% → Emergency */
#define SUMP_PUMP_CURRENT_THRESH 50     /* 0.5A → pump running (×0.01A) */
#define SUMP_PUMP_OVERLOAD_MA    500    /* 5.0A → overload (×0.01A) */
#define SUMP_BATTERY_LOW_V       110    /* 11.0V → low battery (×0.1V) */

/* === Soil Moisture Calibration === */
#define SOIL_AIR_VALUE       800   /* FDC2214 raw reading in air */
#define SOIL_WATER_VALUE     350   /* FDC2214 raw reading in water */
#define SOIL_SAT_THRESHOLD   85.0  /* % VWC — above this = saturated */
#define SOIL_HIGH_THRESHOLD  70.0  /* % VWC — high groundwater */

/* === Battery Thresholds === */
#define BAT_LOW_MV          280    /* 2.80V → low battery alert */
#define BAT_CRIT_MV         250    /* 2.50V → critical, enter low-power mode */
#define BAT_FULL_MV         365    /* 3.65V → fully charged (LiFePO4) */

/* === Storm Mode === */
#define STORM_SCORE_THRESHOLD  55  /* StormSync Score above this = storm mode */

#endif /* STORMSYNC_CONFIG_H */
/*
 * LawnSync — Configuration Constants
 * Pin assignments, network parameters, calibration defaults
 */
#ifndef LAWNSYNC_CONFIG_H
#define LAWNSYNC_CONFIG_H

/* === Network Parameters === */
#define LS_NET_FREQ_HZ        868000000   /* 868 MHz EU / 915 MHz US */
#define LS_NET_BW_HZ          125000      /* 125 kHz bandwidth */
#define LS_NET_SF             9           /* Spreading factor 9 (balanced range/data rate) */
#define LS_NET_CR             0x01        /* Coding rate 4/5 */
#define LS_NET_PREAMBLE       8           /* Preamble symbols */
#define LS_NET_TX_POWER_DBM   22          /* +22 dBm (SX1262) */
#define LS_NET_AES_KEY        {0x4C,0x61,0x77,0x6E,0x53,0x79,0x6E,0x63, \
                                0x4E,0x65,0x74,0x77,0x6F,0x72,0x6B,0x21}

/* === Hub (ESP32-S3) Pin Map === */
#define HUB_GPIO_SX_DIO0      4
#define HUB_GPIO_SX_DIO1      5
#define HUB_GPIO_SX_DIO2      6
#define HUB_GPIO_SX_NSS       7
#define HUB_GPIO_SX_RST       8
#define HUB_GPIO_SX_SCK       9
#define HUB_GPIO_SX_MISO      10
#define HUB_GPIO_SX_MOSI     11
#define HUB_GPIO_BME_SDA     12
#define HUB_GPIO_BME_SCL     13
#define HUB_GPIO_RTC_SDA     14
#define HUB_GPIO_RTC_SCL     15
#define HUB_GPIO_SD_MOSI     16
#define HUB_GPIO_SD_MISO     17
#define HUB_GPIO_SD_SCK      18
#define HUB_GPIO_SD_CS       19
#define HUB_GPIO_LED         20
#define HUB_GPIO_BUZZER      21
#define HUB_GPIO_UART_TX     43
#define HUB_GPIO_UART_RX     44

/* === Soil Node (nRF52840) Pin Map === */
#define SOIL_GPIO_FDC_SCL     2  /* P0.02 */
#define SOIL_GPIO_FDC_SDA     3  /* P0.03 */
#define SOIL_GPIO_DS18B20     4  /* P0.04 */
#define SOIL_GPIO_VEML_SCL    5  /* P0.05 */
#define SOIL_GPIO_VEML_SDA    6  /* P0.06 */
#define SOIL_GPIO_ADC_PH      7  /* P0.07 / AIN7 */
#define SOIL_GPIO_ADC_N       8  /* P0.08 / AIN8 */
#define SOIL_GPIO_ADC_P       9  /* P0.09 / AIN9 */
#define SOIL_GPIO_ADC_K      10  /* P0.10 / AIN10 */
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
#define SOIL_GPIO_ISE_SW     21  /* P0.21 */
#define SOIL_GPIO_VDDH_EN    22  /* P0.22 */

/* === Sprinkler (ESP32) Pin Map === */
#define SPR_GPIO_SX_NSS       4
#define SPR_GPIO_SX_SCK       5
#define SPR_GPIO_SX_MISO     18
#define SPR_GPIO_SX_MOSI     23
#define SPR_GPIO_SX_DIO1     19
#define SPR_GPIO_SX_RST      22
#define SPR_GPIO_SX_BUSY     21
#define SPR_GPIO_ZONE1       25
#define SPR_GPIO_ZONE2       26
#define SPR_GPIO_ZONE3       27
#define SPR_GPIO_ZONE4       14
#define SPR_GPIO_ZONE5       12
#define SPR_GPIO_ZONE6       13
#define SPR_GPIO_ZONE7       15
#define SPR_GPIO_ZONE8        2
#define SPR_GPIO_MASTER      17
#define SPR_GPIO_FLOW        34
#define SPR_GPIO_RAIN        35
#define SPR_GPIO_PRESSURE    36
#define SPR_GPIO_LED         33
#define SPR_GPIO_BUZZER      32
#define SPR_NUM_ZONES         8

/* === Weather Station (ESP32-S3) Pin Map === */
#define WX_GPIO_BME_SDA      4
#define WX_GPIO_BME_SCL      5
#define WX_GPIO_UV_SDA       6
#define WX_GPIO_UV_SCL       7
#define WX_GPIO_WIND_SPD     8
#define WX_GPIO_WIND_DIR     9
#define WX_GPIO_RAIN_TIP    10
#define WX_GPIO_SOLAR_IRR   11
#define WX_GPIO_SX_NSS      12
#define WX_GPIO_SX_SCK      13
#define WX_GPIO_SX_MISO     14
#define WX_GPIO_SX_MOSI     15
#define WX_GPIO_SX_DIO1     16
#define WX_GPIO_SX_RST      17
#define WX_GPIO_SX_BUSY     18
#define WX_GPIO_VBAT        19
#define WX_GPIO_LED         20

/* === Scanner (ESP32-S3) Pin Map === */
#define SCAN_GPIO_CAM_D0     4
#define SCAN_GPIO_CAM_D1     5
#define SCAN_GPIO_CAM_D2     6
#define SCAN_GPIO_CAM_D3     7
#define SCAN_GPIO_CAM_D4     8
#define SCAN_GPIO_CAM_D5     9
#define SCAN_GPIO_CAM_D6    10
#define SCAN_GPIO_CAM_D7    11
#define SCAN_GPIO_CAM_PCLK  12
#define SCAN_GPIO_CAM_HSYNC 13
#define SCAN_GPIO_CAM_VSYNC 14
#define SCAN_GPIO_CAM_XCLK  15
#define SCAN_GPIO_CAM_SDA   16
#define SCAN_GPIO_CAM_SCL   17
#define SCAN_GPIO_CAM_PWDN  18
#define SCAN_GPIO_CAM_RST   19
#define SCAN_GPIO_NIR_LED   20
#define SCAN_GPIO_WHT_LED   21
#define SCAN_GPIO_TSL_SDA   22
#define SCAN_GPIO_TSL_SCL   23
#define SCAN_GPIO_IMU_SDA   24
#define SCAN_GPIO_IMU_SCL   25
#define SCAN_GPIO_GPS_TX    26
#define SCAN_GPIO_GPS_RX    27
#define SCAN_GPIO_SX_NSS    28
#define SCAN_GPIO_SX_SCK    29
#define SCAN_GPIO_SX_MISO   30
#define SCAN_GPIO_SX_MOSI   31
#define SCAN_GPIO_SX_DIO1   32
#define SCAN_GPIO_SX_RST    33
#define SCAN_GPIO_SX_BUSY   34
#define SCAN_GPIO_VBAT      35
#define SCAN_GPIO_LED       36
#define SCAN_GPIO_SHUTTER   37

/* === Sampling Intervals (seconds) === */
#define SOIL_SAMPLE_INTERVAL    900   /* 15 minutes */
#define WEATHER_SAMPLE_INTERVAL 300   /* 5 minutes */
#define SCANNER_SCAN_INTERVAL   86400 /* 24 hours */
#define HEARTBEAT_INTERVAL      3600  /* 1 hour */

/* === Soil Moisture Calibration === */
#define SOIL_AIR_VALUE       800   /* FDC2214 raw reading in air */
#define SOIL_WATER_VALUE     350   /* FDC2214 raw reading in water */
#define SOIL_WILTING_POINT   12.0  /* % VWC — below this = drought stress */
#define SOIL_FIELD_CAPACITY  30.0  /* % VWC — above this = saturated */
#define SOIL_OPTIMAL_LOW     18.0  /* % VWC */
#define SOIL_OPTIMAL_HIGH    28.0  /* % VWC */

/* === pH Calibration === */
#define PH_ADC_4_0           1640  /* ADC raw at pH 4.0 buffer */
#define PH_ADC_7_0           820  /* ADC raw at pH 7.0 buffer */

/* === Sprinkler Safety === */
#define SPR_MAX_FLOW_RATE    300   /* 30.0 L/min (×0.1) */
#define SPR_MIN_FLOW_RATE    10    /* 1.0 L/min during active zone */
#define SPR_MAX_PRESSURE    7000   /* 700.0 kPa (×0.1) */
#define SPR_FREEZE_TEMP       20    /* 2.0°C (×0.1) */
#define SPR_MAX_ZONE_RUNTIME 1800  /* 30 minutes max per zone */

/* === Battery Thresholds === */
#define BAT_LOW_MV          280    /* 2.80V → low battery alert */
#define BAT_CRIT_MV         250    /* 2.50V → critical, enter low-power mode */
#define BAT_FULL_MV         365    /* 3.65V → fully charged (LiFePO4) */

#endif /* LAWNSYNC_CONFIG_H */
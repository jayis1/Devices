/*
 * MosquitoSync — Configuration Constants
 * Pin assignments, network parameters, calibration defaults
 */
#ifndef MOSQUITOSYNC_CONFIG_H
#define MOSQUITOSYNC_CONFIG_H

/* === Network Parameters === */
#define MS_NET_FREQ_HZ        868000000   /* 868 MHz EU / 915 MHz US */
#define MS_NET_BW_HZ          125000      /* 125 kHz bandwidth */
#define MS_NET_SF             9           /* Spreading factor 9 */
#define MS_NET_CR             0x01        /* Coding rate 4/5 */
#define MS_NET_PREAMBLE       8           /* Preamble symbols */
#define MS_NET_TX_POWER_DBM   22          /* +22 dBm (SX1262) */
#define MS_NET_AES_KEY        {0x4D,0x6F,0x73,0x71,0x75,0x69,0x74,0x6F, \
                                0x53,0x79,0x6E,0x63,0x4E,0x65,0x74,0x21}

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

/* === Acoustic Sentinel (ESP32-S3) Pin Map === */
#define ACOUSTIC_GPIO_SX_DIO1     4
#define ACOUSTIC_GPIO_SX_BUSY     5
#define ACOUSTIC_GPIO_SX_NSS      6
#define ACOUSTIC_GPIO_SX_RST      7
#define ACOUSTIC_GPIO_SX_SCK      8
#define ACOUSTIC_GPIO_SX_MISO     9
#define ACOUSTIC_GPIO_SX_MOSI    10
#define ACOUSTIC_GPIO_I2S_BCLK   11
#define ACOUSTIC_GPIO_I2S_LRCLK  12
#define ACOUSTIC_GPIO_I2S_DATA   13
#define ACOUSTIC_GPIO_SHT_SDA    14
#define ACOUSTIC_GPIO_SHT_SCL    15
#define ACOUSTIC_GPIO_VBAT       16
#define ACOUSTIC_GPIO_LED        17
#define ACOUSTIC_GPIO_USB_PWR    18
#define ACOUSTIC_GPIO_MIC_EN     19

/* === CO2 Trap (ESP32-S3) Pin Map === */
#define TRAP_GPIO_SX_DIO1      4
#define TRAP_GPIO_SX_BUSY      5
#define TRAP_GPIO_SX_NSS       6
#define TRAP_GPIO_SX_RST       7
#define TRAP_GPIO_SX_SCK       8
#define TRAP_GPIO_SX_MISO      9
#define TRAP_GPIO_SX_MOSI     10
#define TRAP_GPIO_BME_SDA     11
#define TRAP_GPIO_BME_SCL     12
/* OV2640 camera parallel bus */
#define TRAP_GPIO_CAM_D7      13
#define TRAP_GPIO_CAM_D6      14
#define TRAP_GPIO_CAM_D5      15
#define TRAP_GPIO_CAM_D4      16
#define TRAP_GPIO_CAM_VSYNC   17
#define TRAP_GPIO_CAM_HREF    18
#define TRAP_GPIO_CAM_PCLK    19
#define TRAP_GPIO_CAM_XCLK    20
#define TRAP_GPIO_CAM_SIOC    21
#define TRAP_GPIO_CAM_SIOD    26
#define TRAP_GPIO_IR_BEAM     33
#define TRAP_GPIO_RAIN_TIP    34
#define TRAP_GPIO_PROPANE     35  /* Relay: propane valve control */
#define TRAP_GPIO_FAN_PWM     36  /* PWM: suction fan speed */
#define TRAP_GPIO_HEATER_PWM  37  /* PWM: PTC heat element */
#define TRAP_GPIO_VBAT        38
#define TRAP_GPIO_VSOL        39
#define TRAP_GPIO_TRAP_FULL   40

/* === Window Barrier (ESP32) Pin Map === */
#define BARRIER_GPIO_SX_NSS       4
#define BARRIER_GPIO_SX_SCK       5
#define BARRIER_GPIO_SX_MISO     18
#define BARRIER_GPIO_SX_MOSI     23
#define BARRIER_GPIO_SX_DIO1     19
#define BARRIER_GPIO_SX_RST      22
#define BARRIER_GPIO_SX_BUSY     21
#define BARRIER_GPIO_MOT_AIN1    25  /* DRV8833: close direction */
#define BARRIER_GPIO_MOT_AIN2    26  /* DRV8833: open direction */
#define BARRIER_GPIO_MOT_EN      27  /* DRV8833: nSLEEP */
#define BARRIER_GPIO_REED_CLOSED 14  /* Reed: screen fully closed */
#define BARRIER_GPIO_REED_OPEN   12  /* Reed: screen fully open */
#define BARRIER_GPIO_OVERRIDE    13  /* Manual override button */
#define BARRIER_GPIO_VBAT        32  /* ADC1_CH4 */
#define BARRIER_GPIO_MOT_CURRENT 33  /* ADC1_CH5 (stall detection) */
#define BARRIER_GPIO_VSOL        34  /* Input only (ADC) */
#define BARRIER_GPIO_LED         35

/* === Weather Sentinel (nRF52840) Pin Map === */
#define WX_GPIO_BME_SCL      2  /* P0.02 */
#define WX_GPIO_BME_SDA       3  /* P0.03 */
#define WX_GPIO_WIND_SPD      4  /* P0.04 */
#define WX_GPIO_WIND_DIR      5  /* P0.05 / AIN5 */
#define WX_GPIO_RAIN_TIP      6  /* P0.06 */
#define WX_GPIO_SX_NSS     11  /* P0.11 */
#define WX_GPIO_SX_SCK     12  /* P0.12 */
#define WX_GPIO_SX_MISO    13  /* P0.13 */
#define WX_GPIO_SX_MOSI    14  /* P0.14 */
#define WX_GPIO_SX_DIO1    15  /* P0.15 */
#define WX_GPIO_SX_RST     16  /* P0.16 */
#define WX_GPIO_SX_BUSY    17  /* P0.17 */
#define WX_GPIO_VBAT       18  /* P0.18 / AIN18 */
#define WX_GPIO_VSOL       19  /* P0.19 / AIN19 */
#define WX_GPIO_LED        20  /* P0.20 */

/* === Sampling Intervals (seconds) === */
#define ACOUSTIC_WINDOW_MS       1000  /* 1-second audio window (active) */
#define ACOUSTIC_WINDOW_IDLE_MS  5000  /* 5-second window (idle, duty-cycled) */
#define ACOUSTIC_IDLE_TIMEOUT_S  300   /* 5 min no detection → idle mode */
#define TRAP_SAMPLE_INTERVAL      900   /* 15 minutes (telemetry) */
#define WEATHER_SAMPLE_INTERVAL   300   /* 5 minutes */
#define HEARTBEAT_INTERVAL        3600  /* 1 hour */

/* === Acoustic Detection Thresholds === */
#define ACOUSTIC_CONFIDENCE_PCT   70    /* WingNet confidence > 70% → detection */
#define ACOUSTIC_AUDIO_ENERGY_MIN 50    /* Minimum audio energy to run inference */

/* === Species Classes (WingNet) === */
#define SPECIES_AE_AEG    0   /* Aedes aegypti — Dengue/Zika/Yellow Fever */
#define SPECIES_AE_ALB    1   /* Aedes albopictus — Dengue/Chikungunya */
#define SPECIES_AN_GAM    2   /* Anopheles gambiae — Malaria */
#define SPECIES_AN_STE    3   /* Anopheles stephensi — Malaria */
#define SPECIES_CX_QUI    4   /* Culex quinquefasciatus — West Nile */
#define SPECIES_CX_PIP    5   /* Culex pipiens — West Nile */
#define SPECIES_MAN_UNI   6   /* Mansonia uniformis — Filariasis */
#define SPECIES_NON MOZ   7   /* Non-mosquito */

/* Disease-vector species (classes 0–5 trigger high-risk mode) */
#define IS_DISEASE_VECTOR(c)  ((c) <= 5)

/* === Trap Safety Thresholds === */
#define TRAP_HEATER_MAX_C        70    /* Overheat shutoff (°C) */
#define TRAP_HEATER_TARGET_C     37    /* Human body temp mimic (°C) */
#define TRAP_FAN_DEFAULT_PCT     80    /* Default fan speed (%) */
#define TRAP_PROPANE_LOW_PCT     15    /* Propane low alert */
#define TRAP_BAG_FULL_PCT        90    /* Catch bag full alert */
#define TRAP_RAIN_PAUSE_MMH      10    /* Rain rate to pause camera (mm/h) */

/* === Window Barrier Safety === */
#define BARRIER_MOTOR_STALL_MA   150   /* 1.5A stall threshold (×0.01A) */
#define BARRIER_AUTO_OPEN_TIMEOUT_S  1800  /* 30 min auto-open after auto-close */
#define BARRIER_BATTERY_LOW_V    330   /* 3.30V → low battery (×0.01V) */

/* === BiteRisk & Disease Risk Thresholds === */
#define DISEASE_RISK_HIGH        51    /* High-risk mode activation */
#define DISEASE_RISK_CRITICAL    76    /* Critical: community alert */
#define BITE_RISK_ELEVATED       50    /* Elevated: personal protection advised */

/* === Battery Thresholds === */
#define BAT_LOW_MV          330    /* 3.30V → low battery alert (LiPo) */
#define BAT_CRIT_MV         300    /* 3.00V → critical, enter low-power mode */
#define BAT_FULL_MV         420    /* 4.20V → fully charged (LiPo) */
#define BAT_LIFEPO4_LOW_MV  280    /* 2.80V → low (LiFePO4) */
#define BAT_LIFEPO4_FULL_MV 365    /* 3.65V → full (LiFePO4) */

#endif /* MOSQUITOSYNC_CONFIG_H */
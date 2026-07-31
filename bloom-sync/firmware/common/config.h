/*
 * BloomSync — Global Configuration
 * Pin assignments, RF parameters, and constants for all nodes.
 */
#ifndef BLOOMSYNC_CONFIG_H
#define BLOOMSYNC_CONFIG_H

/* === Network constants === */
#define BS_BAND_2_4GHZ         2402000000UL  /* BLE channel 0 */
#define BS_BLE_CONN_INTERVAL_MS 20
#define BS_BLE_MAX_PERIPHERALS  7
#define BS_AES_KEY_LEN          16
#define BS_CRC_POLY             0x1021  /* CRC-16-CCITT */
#define BS_MAX_PAYLOAD          240
#define BS_MAX_MSG              256
#define BS_MAX_NODES            16

/* === Sampling rates === */
#define BS_VITALS_SAMPLE_HZ     1     /* PPG HR/HRV/SpO₂ */
#define BS_IMU_SAMPLE_HZ        50    /* Activity/sleep IMU */
#define BS_NURSING_TEMP_HZ      0    /* 0.1 Hz — every 10s */
#define BS_NURSING_IMU_HZ       12   /* 12.5 Hz position detection */
#define BS_WOUND_TEMP_HZ        0    /* 0.1 Hz — every 10s */
#define BS_WOUND_MOISTURE_HZ    0    /* 0.05 Hz — every 20s */
#define BS_WOUND_PH_HZ          0    /* 0.05 Hz — every 20s */
#define BS_VOICE_SAMPLE_HZ      16000 /* I²S mic for PPD screening */
#define BS_VOICE_DURATION_S     30    /* 30s sample for prosody extraction */

/* === Timing === */
#define BS_HEARTBEAT_INTERVAL_S 30
#define BS_RECOVERY_WINDOW_DAYS 42   /* 6-week postpartum period */
#define BS_OTA_BLOCK_SIZE       128

/* === Node types === */
enum bs_node_type {
    BS_NODE_HUB            = 0x01,
    BS_NODE_RECOVERY_BAND  = 0x02,
    BS_NODE_NURSING_SENSOR = 0x03,
    BS_NODE_WOUND_PATCH    = 0x04,
};

/* === Hub pin assignments (ESP32-S3) === */
#define HUB_GPIO_I2C_SDA       8
#define HUB_GPIO_I2C_SCL       9
#define HUB_GPIO_TFT_SCK       36
#define HUB_GPIO_TFT_MOSI      37
#define HUB_GPIO_TFT_CS        38
#define HUB_GPIO_TFT_DC        39
#define HUB_GPIO_TFT_RST       40
#define HUB_GPIO_TFT_BL        41
#define HUB_GPIO_I2S_BCLK      42
#define HUB_GPIO_I2S_LRCK      43
#define HUB_GPIO_I2S_DIN       44   /* I²S mic data in */
#define HUB_GPIO_I2S_DOUT      45   /* I²S amp data out */
#define HUB_GPIO_SD_CS         10
#define HUB_GPIO_SD_SCK        12
#define HUB_GPIO_SD_MOSI       11
#define HUB_GPIO_SD_MISO       13
#define HUB_GPIO_LED_STATUS    46
#define HUB_GPIO_HAPTIC_EN     47
#define HUB_GPIO_BUTTON        0

/* === Recovery Band pin assignments (nRF52840) === */
#define RB_GPIO_MAX30101_INT   2   /* P0.02 */
#define RB_GPIO_MAX30101_SCL   3   /* P0.03 — shared I²C */
#define RB_GPIO_MAX30101_SDA   4   /* P0.04 — shared I²C */
#define RB_GPIO_IMU_INT1       5   /* P0.05 */
#define RB_GPIO_IMU_CS         6   /* P0.06 — SPI */
#define RB_GPIO_SPI_SCK        7   /* P0.07 */
#define RB_GPIO_SPI_MISO       8   /* P0.08 */
#define RB_GPIO_SPI_MOSI       9   /* P0.09 */
#define RB_GPIO_TMP117_INT     10  /* P0.10 — shared I²C */
#define RB_GPIO_FUEL_ALERT     11  /* P0.11 */
#define RB_GPIO_LED            12  /* P0.12 */
#define RB_GPIO_BUTTON         13  /* P0.13 */
#define RB_GPIO_CHARGE_STAT    14  /* P0.14 */
#define RB_GPIO_VBAT_SENSE     15  /* P0.15 — ADC for battery */

/* === Nursing Sensor pin assignments (nRF52840) === */
#define NS_GPIO_TMP_L_INT      2   /* P0.02 — left breast TMP117 */
#define NS_GPIO_TMP_R_INT      3   /* P0.03 — right breast TMP117 */
#define NS_GPIO_I2C_SDA        4   /* P0.04 — shared I²C for both TMP117 */
#define NS_GPIO_I2C_SCL        5   /* P0.05 */
#define NS_GPIO_IMU_INT1       6   /* P0.06 — LIS2DW12 */
#define NS_GPIO_LED            7   /* P0.07 */
#define NS_GPIO_BUTTON         8   /* P0.08 */

/* === Wound Patch pin assignments (nRF52840) === */
#define WP_GPIO_TMP_INT        2   /* P0.02 — TMP117 wound temp */
#define WP_GPIO_I2C_SDA        3   /* P0.03 — shared I²C */
#define WP_GPIO_I2C_SCL        4   /* P0.04 */
#define WP_GPIO_FDC_INT        5   /* P0.05 — FDC2214 moisture interrupt */
#define WP_GPIO_LMP_ALERT      6   /* P0.06 — LMP91200 pH alert */
#define WP_GPIO_PH_ADC         7   /* P0.07 — ADC for pH analog output */
#define WP_GPIO_LED            8   /* P0.08 */
#define WP_GPIO_BUTTON         9   /* P0.09 */

/* === I²C addresses === */
#define BS_I2C_MAX30101        0x57
#define BS_I2C_TMP117          0x48
#define BS_I2C_LIS2DW12        0x1E  /* (SA0=0) */
#define BS_I2C_BME280          0x76
#define BS_I2C_DS3231          0x68
#define BS_I2C_DRV2605L        0x5A
#define BS_I2C_MAX17048        0x36
#define BS_I2C_FDC2214         0x2A  /* FDC2214 default */
#define BS_I2C_LMP91200        0x00  /* Analog front-end, no I²C addr, uses GPIO */

/* === Hemorrhage risk thresholds (edge screening) === */
#define BS_HR_HIGH_THRESHOLD       110   /* bpm — tachycardia */
#define BS_HR_LOW_THRESHOLD        45    /* bpm — bradycardia */
#define BS_SPO2_LOW_THRESHOLD      92    /* % — hypoxemia */
#define BS_TEMP_HIGH_THRESHOLD     382   /* 38.2°C — fever, centi-degrees */
#define BS_HR_RISE_RATE_THRESHOLD  15    /* bpm rise in 30 min — hemorrhage indicator */
#define BS_HRV_DROP_THRESHOLD      50    /* % RMSSD drop in 1h — shock indicator */

/* === Mastitis thresholds === */
#define BS_BREAST_TEMP_ASYM_THRESHOLD  13  /* 1.3°C × 10 — clinical mastitis threshold */
#define BS_BREAST_TEMP_HIGH_THRESHOLD  375 /* 37.5°C absolute — localized inflammation */

/* === Wound infection thresholds === */
#define BS_WOUND_TEMP_HIGH_THRESHOLD   379 /* 37.9°C — local inflammation */
#define BS_WOUND_TEMP_RISE_THRESHOLD   8   /* 0.8°C rise in 12h — infection trend */
#define BS_WOUND_PH_HIGH_THRESHOLD     75  /* pH 7.5 — bacterial growth indicator */
#define BS_WOUND_MOISTURE_HIGH_THRESHOLD 80 /* % — excessive exudate */

#endif /* BLOOMSYNC_CONFIG_H */
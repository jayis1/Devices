/*
 * MenoSync — Global Configuration
 * Pin assignments, RF parameters, and constants for all nodes.
 */
#ifndef MENOSYNC_CONFIG_H
#define MENOSYNC_CONFIG_H

/* === Network constants === */
#define MS_BAND_2_4GHZ         2402000000UL  /* BLE channel 0 */
#define MS_BLE_CONN_INTERVAL_MS 20
#define MS_BLE_MAX_PERIPHERALS  7
#define MS_AES_KEY_LEN          16
#define MS_CRC_POLY             0x1021  /* CRC-16-CCITT */
#define MS_MAX_PAYLOAD          240
#define MS_MAX_MSG              256
#define MS_MAX_NODES            32

/* === Sub-GHz 868 MHz mesh === */
#define MS_SUBGHZ_FREQ          868000000UL
#define MS_SUBGHZ_BPS           100000UL
#define MS_SUBGHZ_TX_POWER_DBM  20
#define MS_SUBGHZ_TDMA_SLOTS    16
#define MS_SUBGHZ_SLOT_MS       500
#define MS_SUBGHZ_SUPERFRAME_MS 8000  /* 16 × 500 */

/* === Sampling rates === */
#define MS_VITALS_SAMPLE_HZ     1     /* PPG HR/HRV/SpO₂ */
#define MS_EDA_SAMPLE_HZ        4     /* EDA skin conductance */
#define MS_IMU_SAMPLE_HZ        50    /* Activity/sleep IMU */
#define MS_SKIN_TEMP_HZ         0    /* 0.1 Hz — every 10s */
#define MS_BCG_SAMPLE_HZ        1     /* Bed mat ballistocardiography */
#define MS_SWEAT_SAMPLE_HZ      0    /* 0.05 Hz — every 20s */
#define MS_BED_TEMP_HZ          0    /* 0.05 Hz — every 20s */
#define MS_AMBIENT_SAMPLE_HZ    0    /* 0.1 Hz — every 10s */
#define MS_RADIANT_SAMPLE_HZ    0    /* 0.02 Hz — every 50s */
#define MS_VOICE_SAMPLE_HZ      16000 /* I²S mic for mood screening */
#define MS_VOICE_DURATION_S     30    /* 30s sample for prosody extraction */

/* === Timing === */
#define MS_HEARTBEAT_INTERVAL_S 30
#define MS_HOTFLASH_WINDOW_MIN  20   /* 20-min prediction window */
#define MS_OTA_BLOCK_SIZE       128

/* === Node types === */
enum ms_node_type {
    MS_NODE_HUB          = 0x01,
    MS_NODE_WRIST_BAND   = 0x02,
    MS_NODE_BED_MAT      = 0x03,
    MS_NODE_CLIMATE      = 0x04,
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
/* Sub-GHz RFM69HCW SPI pins */
#define HUB_GPIO_RFM_CS        14
#define HUB_GPIO_RFM_SCK       12
#define HUB_GPIO_RFM_MOSI      11
#define HUB_GPIO_RFM_MISO      13
#define HUB_GPIO_RFM_RST       15
#define HUB_GPIO_RFM_DIO0      16  /* Interrupt */

/* === Wrist Band pin assignments (nRF52840) === */
#define WB_GPIO_MAX30101_INT   2   /* P0.02 */
#define WB_GPIO_I2C_SDA        3   /* P0.03 — shared I²C */
#define WB_GPIO_I2C_SCL        4   /* P0.04 — shared I²C */
#define WB_GPIO_IMU_INT1       5   /* P0.05 */
#define WB_GPIO_IMU_CS         6   /* P0.06 — SPI */
#define WB_GPIO_SPI_SCK        7   /* P0.07 */
#define WB_GPIO_SPI_MISO       8   /* P0.08 */
#define WB_GPIO_SPI_MOSI       9   /* P0.09 */
#define WB_GPIO_TMP117_INT     10  /* P0.10 — shared I²C */
#define WB_GPIO_ADS_DRDY       11  /* P0.11 — ADS1292 data ready */
#define WB_GPIO_ADS_CS         12  /* P0.12 — ADS1292 SPI CS */
#define WB_GPIO_FUEL_ALERT     13  /* P0.13 */
#define WB_GPIO_LED            14  /* P0.14 */
#define WB_GPIO_BUTTON         15  /* P0.15 */
#define WB_GPIO_CHARGE_STAT    16  /* P0.16 */
#define WB_GPIO_VBAT_SENSE     17  /* P0.17 — ADC for battery */
#define WB_GPIO_EDA_ELECTRODE_A 20 /* P0.20 — EDA electrode A */
#define WB_GPIO_EDA_ELECTRODE_B 21 /* P0.21 — EDA electrode B */

/* === Bed Mat pin assignments (nRF52840) === */
#define BM_GPIO_PIEZO_ADC      2   /* P0.02 — ADC for PVDF piezo signal */
#define BM_GPIO_I2C_SDA        3   /* P0.03 — shared I²C */
#define BM_GPIO_I2C_SCL        4   /* P0.04 — shared I²C */
#define BM_GPIO_FDC_INT        5   /* P0.05 — FDC2214 sweat interrupt */
#define BM_GPIO_TMP_INT        6   /* P0.06 — TMP117 temp alert */
#define WB_GPIO_LED            7   /* P0.07 */
#define BM_GPIO_BUTTON         8   /* P0.08 */

/* === Climate Node pin assignments (ESP32-C3) === */
#define CN_GPIO_I2C_SDA        4
#define CN_GPIO_I2C_SCL        5
#define CN_GPIO_RFM_CS         7
#define CN_GPIO_RFM_SCK        8
#define CN_GPIO_RFM_MOSI       9
#define CN_GPIO_RFM_MISO       10
#define CN_GPIO_RFM_RST        11
#define CN_GPIO_RFM_DIO0       12  /* Interrupt */
#define CN_GPIO_RELAY_HVAC     2
#define CN_GPIO_RELAY_SHADE    3
#define CN_GPIO_MLX_INT        6
#define CN_GPIO_LED            7

/* === I²C addresses === */
#define MS_I2C_MAX30101        0x57
#define MS_I2C_TMP117          0x48
#define MS_I2C_LSM6DSO         0x6A
#define MS_I2C_BME280          0x76
#define MS_I2C_DS3231          0x68
#define MS_I2C_DRV2605L        0x5A
#define MS_I2C_MAX17048        0x36
#define MS_I2C_FDC2214         0x2A
#define MS_I2C_MLX90640        0x33

/* === Hot flash thresholds (edge screening) === */
#define MS_SKIN_TEMP_RISE_CD    30    /* 0.3°C rise → hot flash precursor */
#define MS_SKIN_TEMP_HOT_CD     3740  /* 37.4°C absolute skin temp */
#define MS_EDA_SPIKE_UV         250   /* 2.5 µS EDA spike → sympathetic surge */
#define MS_HR_RISE_THRESHOLD    12    /* bpm rise in 15 min */
#define MS_AMBIENT_TEMP_HIGH    260   /* 26.0°C ambient → trigger risk */

/* === Night sweat thresholds === */
#define MS_SWEAT_MOISTURE_THRESHOLD  35  /* % moisture above baseline */
#define MS_BED_TEMP_HIGH_CD          368 /* 36.8°C mattress temp */

/* === Cooling thresholds === */
#define MS_COOLING_TRIGGER_RISK      65  /* Hot flash risk > 65% → pre-cool */
#define MS_COOLING_PRE_MINUTES       8   /* Pre-cool 8 min before predicted onset */
#define MS_COOLING_TEMP_TARGET_CD    220 /* 22.0°C target ambient during cooling */
#define MS_SHADE_CLOSE_RISK          50  /* Close shades if risk > 50% (sunlight = trigger) */

#endif /* MENOSYNC_CONFIG_H */
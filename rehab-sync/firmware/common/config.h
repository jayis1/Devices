/*
 * RehabSync — Global Configuration
 * Pin assignments, RF parameters, and constants for all nodes.
 */
#ifndef REHABSYNC_CONFIG_H
#define REHABSYNC_CONFIG_H

/* === Network constants === */
#define RS_BAND_868MHZ          868000000UL
#define RS_SF                   7
#define RS_BW                   250000
#define RS_TX_POWER             22
#define RS_PREAMBLE_LEN         8
#define RS_MAX_PAYLOAD          240
#define RS_MAX_MSG              256
#define RS_MAX_NODES            16
#define RS_AES_KEY_LEN          16
#define RS_CRC_POLY             0x1021  /* CRC-16-CCITT */

/* === BLE BAN constants === */
#define RS_BLE_CONN_INTERVAL_MS 10
#define RS_BLE_MAX_PERIPHERALS  7
#define RS_IMU_SAMPLE_HZ        100
#define RS_FORCE_SAMPLE_HZ      50
#define RS_PRESSURE_FRAME_HZ    30

/* === Timing === */
#define RS_TDMA_SLOT_MS         2000
#define RS_HEARTBEAT_INTERVAL_S 30
#define RS_SESSION_TIMEOUT_S    300
#define RS_OTA_BLOCK_SIZE       128

/* === Node types === */
enum rs_node_type {
    RS_NODE_HUB           = 0x01,
    RS_NODE_BODY_SENSOR   = 0x02,
    RS_NODE_SMART_BAND    = 0x03,
    RS_NODE_PRESSURE_MAT  = 0x04,
};

/* === Hub pin assignments (ESP32-S3) === */
#define HUB_GPIO_SX_DIO1       4
#define HUB_GPIO_SX_BUSY       5
#define HUB_GPIO_SX_NSS        6
#define HUB_GPIO_SX_RST        7
#define HUB_GPIO_SX_SCK        8
#define HUB_GPIO_SX_MISO       9
#define HUB_GPIO_SX_MOSI       10
#define HUB_GPIO_I2C_SDA       11
#define HUB_GPIO_I2C_SCL       12
#define HUB_GPIO_HUB_IMU_INT   13
#define HUB_GPIO_HUB_IMU_CS    14
#define HUB_GPIO_HUB_IMU_SCK   15
#define HUB_GPIO_HUB_IMU_MISO  16
#define HUB_GPIO_HUB_IMU_MOSI  17
#define HUB_GPIO_SD_CS         18
#define HUB_GPIO_SD_SCK        19
#define HUB_GPIO_SD_MOSI       20
#define HUB_GPIO_SD_MISO       21
#define HUB_GPIO_TFT_SCK       35
#define HUB_GPIO_TFT_MOSI      36
#define HUB_GPIO_TFT_CS        37
#define HUB_GPIO_TFT_DC        38
#define HUB_GPIO_TFT_RST       39
#define HUB_GPIO_TFT_BL        40
#define HUB_GPIO_I2S_BCLK      41
#define HUB_GPIO_I2S_LRCK      42
#define HUB_GPIO_I2S_DIN       45
#define HUB_GPIO_LED_STATUS    46

/* === Body Sensor pin assignments (nRF52840) === */
#define BS_GPIO_IMU_CS         2   /* P0.02 */
#define BS_GPIO_MAG_CS         3   /* P0.03 */
#define BS_GPIO_SPI_SCK        4   /* P0.04 */
#define BS_GPIO_SPI_MISO       5   /* P0.05 */
#define BS_GPIO_SPI_MOSI       6   /* P0.06 */
#define BS_GPIO_IMU_INT1       7   /* P0.07 */
#define BS_GPIO_MAG_INT        8   /* P0.08 */
#define BS_GPIO_LED            9   /* P0.09 */
#define BS_GPIO_BUTTON         11  /* P0.11 */

/* === Smart Band pin assignments (nRF52840) === */
#define SB_GPIO_HX711_SCK      2   /* P0.02 */
#define SB_GPIO_HX711_DOUT     3   /* P0.03 */
#define SB_GPIO_IMU_CS         4   /* P0.04 */
#define SB_GPIO_SPI_SCK        5   /* P0.05 */
#define SB_GPIO_SPI_MISO       6   /* P0.06 */
#define SB_GPIO_SPI_MOSI       7   /* P0.07 */
#define SB_GPIO_IMU_INT1       8   /* P0.08 */
#define SB_GPIO_I2C_SDA        9   /* P0.09 */
#define SB_GPIO_I2C_SCL        10  /* P0.10 */
#define SB_GPIO_LED            11  /* P0.11 */
#define SB_GPIO_BUTTON         13  /* P0.13 */

/* === Pressure Mat pin assignments (ESP32-S3) === */
#define PM_GPIO_SX_DIO1        4
#define PM_GPIO_SX_BUSY        5
#define PM_GPIO_SX_NSS         6
#define PM_GPIO_SX_RST         7
#define PM_GPIO_SX_SCK         8
#define PM_GPIO_SX_MISO        9
#define PM_GPIO_SX_MOSI        10
#define PM_GPIO_I2C_SDA        11
#define PM_GPIO_I2C_SCL        12
#define PM_GPIO_MUX_ROW_S0     13
#define PM_GPIO_MUX_ROW_S1     14
#define PM_GPIO_MUX_ROW_S2     15
#define PM_GPIO_MUX_ROW_S3     16
#define PM_GPIO_MUX_COL_S0     17
#define PM_GPIO_MUX_COL_S1     18
#define PM_GPIO_MUX_COL_S2     19
#define PM_GPIO_MUX_COL_S3     20
#define PM_GPIO_ADC_ALERT      40
#define PM_GPIO_IMU_INT1       41
#define PM_GPIO_LED            42

/* === Pressure Mat dimensions === */
#define PM_ROWS                16
#define PM_COLS                16
#define PM_TOTAL_SENSORS       256

#endif /* REHABSYNC_CONFIG_H */
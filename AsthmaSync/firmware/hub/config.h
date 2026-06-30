/**
 * AsthmaSync — Hub Configuration
 * ESP32-S3-WROOM-1-N16R8
 *
 * License: MIT
 */

#ifndef HUB_CONFIG_H
#define HUB_CONFIG_H

/* ── Platform ──────────────────────────────────────────── */
#define HUB_SOC          "ESP32-S3-WROOM-1-N16R8"
#define HUB_FLASH_SIZE   (16 * 1024 * 1024)   /* 16 MB */
#define HUB_PSRAM_SIZE   (8 * 1024 * 1024)    /* 8 MB */

/* ── WiFi ──────────────────────────────────────────────── */
#define WIFI_SSID        "AsthmaSync-Hub"
#define WIFI_PASSWORD    "asthmasync-2024"
#define WIFI_MAX_RETRY   5

/* ── MQTT ─────────────────────────────────────────────── */
#define MQTT_BROKER_URL  "mqtts://broker.asthmasync.io:8883"
#define MQTT_CLIENT_ID   "asthmasync-hub-001"
#define MQTT_TOPIC_TELEMETRY  "asthmasync/telemetry"
#define MQTT_TOPIC_EVENTS     "asthmasync/events"
#define MQTT_TOPIC_COMMANDS   "asthmasync/commands"
#define MQTT_OFFLINE_QUEUE_SIZE  512   /* packets buffered in PSRAM */

/* ── BLE (Central) ─────────────────────────────────────── */
#define BLE_SCAN_DURATION_MS  5000
#define BLE_CONN_INTERVAL_MS  30    /* 37.5 ms (24 × 1.25ms) */
#define BLE_CONN_TIMEOUT_MS   4000  /* 4 s supervision timeout */

/* AsthmaSync GATT UUIDs */
#define BLE_SVC_ASTHMA        0x1801  /* custom service */
#define BLE_CHAR_TELEMETRY    0x2A01  /* notify */
#define BLE_CHAR_COMMAND      0x2A02  /* write */
#define BLE_CHAR_EVENT        0x2A03  /* notify */

/* ── Edge ML (tflite-micro) ────────────────────────────── */
#define TFLM_ARENA_SIZE       (256 * 1024)  /* 256 KB arena */
#define WHEEZE_MODEL_PATH     "/spiffs/wheeze_cnn.tflite"
#define ACTUATION_MODEL_PATH  "/spiffs/actuation_rf.tflite"

/* ── Display (ILI9341) ────────────────────────────────── */
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_SPI_HZ  (40 * 1000 * 1000)  /* 40 MHz SPI clock */

/* ── GPIO ──────────────────────────────────────────────── */
#define GPIO_SPI_CLK    4
#define GPIO_SPI_MOSI   5
#define GPIO_SPI_MISO   6
#define GPIO_TFT_CS     7
#define GPIO_TFT_DC     11
#define GPIO_TFT_RST   14
#define GPIO_SD_CS     10
#define GPIO_BTN_ACK    1
#define GPIO_BTN_SILENCE 2
#define GPIO_BTN_PAIR  41
#define GPIO_RGB_LED   38
#define GPIO_BUZZER    42

/* ── Alert Thresholds (GINA-aligned) ───────────────────── */
#define THRESH_SPO2_RED      92      /* % — Red Zone */
#define THRESH_SPO2_YELLOW    95      /* % — Yellow Zone */
#define THRESH_PM25_HIGH      35      /* µg/m³ — EPA standard */
#define THRESH_VOC_HIGH       400     /* BME688 IAQ index */
#define THRESH_HRV_DROP_PCT   20      /* % below 7-day baseline */
#define THRESH_RESCUE_YELLOW  2       /* uses/week → Yellow */
#define THRESH_RESCUE_RED     4       /* uses/week → Red */

/* ── Timing ────────────────────────────────────────────── */
#define TELEMETRY_INTERVAL_SEC   30   /* normal: 30 s */
#define TELEMETRY_INTERVAL_ALERT  5   /* alert mode: 5 s */
#define CLOUD_UPLOAD_INTERVAL_SEC 60  /* batch upload */
#define DISPLAY_UPDATE_MS       500
#define HEARTBEAT_INTERVAL_SEC   15

/* ── Cache ────────────────────────────────────────────── */
#define CACHE_RETENTION_DAYS   14    /* 14-day rolling cache in PSRAM */
#define CACHE_MAX_RECORDS      40320 /* 14 days × 60 min × 24 hr (per-minute) */

#endif /* HUB_CONFIG_H */
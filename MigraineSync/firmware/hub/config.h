/**
 * MigraineSync — Hub Configuration
 * =================================
 * ESP32-S3-WROOM-1-N16R8 pin definitions, radio config, MQTT topics.
 *
 * License: MIT
 */

#ifndef HUB_CONFIG_H
#define HUB_CONFIG_H

/* ── Node identity ──────────────────────────────────────── */
#define NODE_ID          0x0001
#define FIRMWARE_VERSION "1.0.0"

/* ── WiFi ───────────────────────────────────────────────── */
#define WIFI_SSID        "MigraineSync-Hub"
#define WIFI_PASS        "changeme-secure"
#define WIFI_TIMEOUT_MS  15000

/* ── MQTT ───────────────────────────────────────────────── */
#define MQTT_BROKER      "broker.migrainesync.io"
#define MQTT_PORT        8883
#define MQTT_TELEMETRY_TOPIC "migrainesync/telemetry"
#define MQTT_EVENTS_TOPIC    "migrainesync/events"
#define MQTT_CLIENT_ID  "migrainesync-hub-001"

/* ── Sub-GHz (SX1262) ───────────────────────────────────── */
#define SUBGHZ_FREQ_EU   868100000   /* 868.1 MHz EU */
#define SUBGHZ_FREQ_US   915000000   /* 915.0 MHz US */
#define SUBGHZ_TX_POWER  22           /* dBm */
#define SUBGHZ_SF        7            /* Spreading factor 7 (fast) */
#define SUBGHZ_BW        125000       /* 125 kHz */
#define SUBGHZ_TX_PIN    15           /* SX1262 CS */
#define SUBGHZ_DIO1_PIN  16
#define SUBGHZ_BUSY_PIN  17
#define SUBGHZ_RST_PIN   18
#define SUBGHZ_MOSI_PIN  8
#define SUBGHZ_MISO_PIN  48
#define SUBGHZ_SCK_PIN   3

/* ── SPI (TFT + SD) ─────────────────────────────────────── */
#define VSPI_CLK_PIN     4
#define VSPI_MOSI_PIN    5
#define VSPI_MISO_PIN    6
#define TFT_CS_PIN       7
#define TFT_DC_PIN       11
#define TFT_RST_PIN      14
#define SD_CS_PIN        10

/* ── GPIO ───────────────────────────────────────────────── */
#define BTN_ACK_PIN      1
#define BTN_SILENCE_PIN  2
#define BTN_PAIR_PIN     41
#define RGB_LED_PIN      38
#define BUZZER_PIN       42

/* ── Edge ML ────────────────────────────────────────────── */
#define EDGE_ML_INTERVAL_S    900   /* 15 minutes */
#define RISK_THRESHOLD_LOW    30.0
#define RISK_THRESHOLD_MOD    55.0
#define RISK_THRESHOLD_HIGH   70.0

/* ── Cache ──────────────────────────────────────────────── */
#define CACHE_MAX_RECORDS  40320    /* 14 days × 24h × 60min / 5min = 4032, ×10 for sensor data */
#define BACKFILL_BATCH     50

/* ── BLE ────────────────────────────────────────────────── */
#define BLE_SCAN_WINDOW_MS  3000
#define BLE_CONN_INTERVAL_MIN_MS  15
#define BLE_CONN_INTERVAL_MAX_MS  100
#define BLE_SERVICE_UUID   0x6E400001

#endif /* HUB_CONFIG_H */
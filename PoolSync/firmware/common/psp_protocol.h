/**
 * @file psp_protocol.h
 * @brief PoolSync Protocol (PSP) — Frame format, message types, encryption
 *
 * Layered on Sub-GHz LoRa (SX1262) + Wi-Fi MQTT
 * Binary, little-endian, CRC16, AES-128-GCM
 */

#ifndef PSP_PROTOCOL_H
#define PSP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CONSTANTS
 * ============================================================ */

#define PSP_PREAMBLE         0x5AA5      /* 2-byte sync preamble */
#define PSP_SYNC_WORD        0x5053      /* "PS" — PoolSync */
#define PSP_MAX_PAYLOAD      200         /* bytes */
#define PSP_HEADER_SIZE      11          /* preamble(2)+sync(2)+len(2)+src(2)+dst(2)+type(1) */
#define PSP_CRC_SIZE         2           /* CRC16 */
#define PSP_FRAME_OVERHEAD   (PSP_HEADER_SIZE + PSP_CRC_SIZE)
#define PSP_MAX_FRAME        (PSP_HEADER_SIZE + PSP_MAX_PAYLOAD + PSP_CRC_SIZE)

/* AES-128-GCM */
#define PSP_AES_KEY_SIZE    16
#define PSP_AES_NONCE_SIZE  12
#define PSP_AES_TAG_SIZE    16

/* Node addresses (2-byte) */
#define PSP_ADDR_HUB                0x0001
#define PSP_ADDR_CHEM_PROBE_BASE    0x0100  /* + probe_id (0x0100–0x01FF) */
#define PSP_ADDR_POOL_CAMERA        0x0200
#define PSP_ADDR_EQUIP_CTRL         0x0300
#define PSP_ADDR_SOLAR_MONITOR      0x0400
#define PSP_ADDR_BROADCAST          0xFFFF

/* ============================================================
 * MESSAGE TYPES
 * ============================================================ */

typedef enum __attribute__((packed)) {
    /* Chemistry */
    PSP_MSG_CHEM_DATA       = 0x01,  /* Probe→Hub: pH, ORP, Cl, temp, cond, turbidity */
    PSP_MSG_CHEM_CALIBRATE  = 0x0F,  /* Hub→Probe: trigger 2-point pH calibration */

    /* Camera */
    PSP_MSG_IMAGE_DATA      = 0x02,  /* Camera→Hub: water clarity metadata */
    PSP_MSG_IMAGE_UPLOAD    = 0x03,  /* Hub→Camera: trigger full image Wi-Fi upload */

    /* Equipment */
    PSP_MSG_EQUIP_STATUS    = 0x04,  /* Equip→Hub: pump/heater/valve/flow/pressure */
    PSP_MSG_DOSE_COMMAND    = 0x05,  /* Hub→Equip: chemical dosing (pump_id, ml) */
    PSP_MSG_EQUIP_COMMAND   = 0x06,  /* Hub→Equip: pump/heater/valve control */

    /* Solar */
    PSP_MSG_SOLAR_DATA      = 0x07,  /* Solar→Hub: irradiance, current, panel temp */

    /* Safety */
    PSP_MSG_ALARM           = 0x08,  /* Any→Hub: safety alarm (entrapment, GFCI, access) */

    /* System */
    PSP_MSG_HEARTBEAT       = 0x10,  /* Any↔Hub: keep-alive, battery, RSSI */
    PSP_MSG_PING            = 0x11,  /* Any→Hub: link quality check */
    PSP_MSG_PONG            = 0x12,  /* Hub→Any: link quality response */
    PSP_MSG_TIME_SYNC       = 0x13,  /* Hub→Any: UTC timestamp sync */

    /* OTA */
    PSP_MSG_OTA_START      = 0x20,  /* Hub→Any: begin OTA, total size + CRC32 */
    PSP_MSG_OTA_CHUNK      = 0x21,  /* Hub→Any: firmware chunk */
    PSP_MSG_OTA_DONE       = 0x22,  /* Any→Hub: update verification result */

    /* Hub internal */
    PSP_MSG_HUB_CONFIG     = 0x30,  /* Cloud→Hub: configuration update */
    PSP_MSG_HUB_STATE      = 0x31,  /* Hub→Cloud: full state snapshot */
} psp_msg_type_t;

/* ============================================================
 * ALARM SUB-TYPES (payload byte 0 of PSP_MSG_ALARM)
 * ============================================================ */

typedef enum {
    PSP_ALARM_ENTRAPMENT    = 0x01,  /* Suction entrapment detected */
    PSP_ALARM_GFCI_FAULT    = 0x02,  /* Ground fault detected */
    PSP_ALARM_UNAUTH_ACCESS = 0x03,  /* Unsupervised pool access */
    PSP_ALARM_CHEM_OUTSIDE  = 0x04,  /* Chemistry dangerously out of range */
    PSP_ALARM_FREEZE        = 0x05,  /* Freeze protection activated */
    PSP_ALARM_EQUIP_FAULT   = 0x06,  /* Equipment malfunction */
    PSP_ALARM_LOW_BATTERY   = 0x07,  /* Probe battery low */
    PSP_ALARM_DOSE_FAIL     = 0x08,  /* Dosing command failed (flow verification) */
} psp_alarm_type_t;

/* ============================================================
 * FRAME STRUCTURE
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint16_t preamble;       /* 0x5AA5 */
    uint16_t sync_word;      /* 0x5053 */
    uint16_t length;         /* total frame length including header+payload+CRC */
    uint16_t src_addr;       /* sender node address */
    uint16_t dst_addr;       /* destination node address (0xFFFF=broadcast) */
    uint8_t  msg_type;       /* psp_msg_type_t */
} psp_header_t;

typedef struct {
    psp_header_t header;
    uint8_t payload[PSP_MAX_PAYLOAD];
    uint16_t payload_len;
    uint16_t crc;           /* CRC16 over header+payload (before encryption) */
} psp_frame_t;

/* ============================================================
 * SPECIFIC PAYLOAD STRUCTURES
 * ============================================================ */

/* PSP_MSG_CHEM_DATA */
typedef struct __attribute__((packed)) {
    float    ph;              /* pH value 0.00–14.00 */
    float    orp_mv;          /* ORP in mV (-2000 to +2000) */
    float    free_cl_ppm;    /* Free chlorine ppm 0.00–10.00 */
    float    temperature_c;   /* Water temperature °C */
    float    conductivity_us; /* Conductivity µS/cm 0–100000 */
    float    turbidity_ntu;   /* Turbidity NTU 0–1000 */
    uint16_t battery_mv;      /* Battery voltage mV */
    int8_t   rssi_dbm;        /* Radio RSSI dBm */
} psp_chem_data_t;

/* PSP_MSG_DOSE_COMMAND */
typedef struct __attribute__((packed)) {
    uint8_t  pump_id;         /* 0=acid, 1=chlorine, 2=clarifier */
    float    volume_ml;       /* Volume to dose in mL */
    uint16   duration_s;      /* Maximum dosing time (safety timeout) */
    uint32_t command_id;      /* Unique command ID for ACK tracking */
} psp_dose_command_t;

/* PSP_MSG_EQUIP_COMMAND */
typedef struct __attribute__((packed)) {
    uint8_t  device_id;       /* 0=pump, 1=heater, 2=pool_light, 3=spa_light, 4=valve1, 5=valve2, 6=blower */
    uint8_t  command;         /* 0=off, 1=on, 2=toggle, 3=set_speed (pump) */
    uint16_t parameter;       /* Speed % for pump, setpoint °C for heater */
    uint32_t duration_s;      /* Auto-off timer (0=permanent) */
} psp_equip_command_t;

/* PSP_MSG_EQUIP_STATUS */
typedef struct __attribute__((packed)) {
    uint8_t  relay_states;    /* Bitmask: bit0=pump, bit1=heater, etc. */
    float    flow_lpm;        /* Flow rate L/min (dosing verification) */
    float    pressure_kpa;    /* Filter pressure kPa (clog + entrapment) */
    float    current_a;       /* AC current A (GFCI monitor) */
    uint8_t  pump_status;     /* 0=idle, 1=running, 2=dosing, 3=fault */
    uint16_t battery_mv;      /* Not used (always mains powered) */
    int8_t   rssi_dbm;       /* Radio RSSI */
} psp_equip_status_t;

/* PSP_MSG_IMAGE_DATA */
typedef struct __attribute__((packed)) {
    float    clarity_score;   /* 0.0–1.0 (on-device computed) */
    float    green_channel;   /* Normalized green channel intensity */
    float    turbidity_ntu;   /* Estimated turbidity from image */
    uint8_t  algae_risk;      /* 0=none, 1=low, 2=medium, 3=high */
    uint16_t image_hash;      /* CRC16 of full image (for Wi-Fi upload matching) */
    uint32_t timestamp;       /* Unix timestamp */
} psp_image_data_t;

/* PSP_MSG_SOLAR_DATA */
typedef struct __attribute__((packed)) {
    float    irradiance_wm2;  /* Solar irradiance W/m² */
    float    panel_current_a; /* Solar pump current A */
    float    panel_temp_c;    /* Panel temperature °C */
    float    roof_temp_c;     /* Roof temperature °C */
    uint16_t battery_mv;
    int8_t   rssi_dbm;
} psp_solar_data_t;

/* PSP_MSG_HEARTBEAT */
typedef struct __attribute__((packed)) {
    uint32_t uptime_s;        /* Seconds since boot */
    uint16_t battery_mv;      /* Battery voltage mV (0 if mains) */
    int8_t   rssi_dbm;        /* Last TX RSSI */
    uint8_t  free_heap_pct;   /* Free heap % */
    uint8_t  node_status;     /* Bitmask: bit0=sensors_ok, bit1=radio_ok, bit2=cloud_ok */
} psp_heartbeat_t;

/* PSP_MSG_ALARM */
typedef struct __attribute__((packed)) {
    uint8_t  alarm_type;      /* psp_alarm_type_t */
    uint8_t  severity;        /* 0=info, 1=warning, 2=critical, 3=emergency */
    float    value;            /* Alarm-specific value */
    uint32_t timestamp;       /* Unix timestamp */
    uint8_t  data[32];         /* Alarm-specific data */
} psp_alarm_payload_t;

/* ============================================================
 * API FUNCTIONS
 * ============================================================ */

/**
 * Encode a PSP frame from header + payload
 * @return frame length in bytes, 0 on error
 */
uint16_t psp_encode(const psp_header_t *header, const uint8_t *payload,
                    uint16_t payload_len, uint8_t *out_buf, uint16_t out_buf_size);

/**
 * Decode a PSP frame from raw bytes
 * @return 0 on success, negative on error
 */
int psp_decode(const uint8_t *raw, uint16_t raw_len,
               psp_frame_t *frame);

/**
 * Calculate CRC16-CCITT over data
 */
uint16_t psp_crc16(const uint8_t *data, uint16_t len);

/**
 * Encrypt payload with AES-128-GCM
 * @return 0 on success, negative on error
 */
int psp_encrypt(const uint8_t *key, const uint8_t *nonce,
                const uint8_t *plaintext, uint16_t pt_len,
                uint8_t *ciphertext, uint8_t *tag);

/**
 * Decrypt payload with AES-128-GCM
 * @return 0 on success, negative on auth failure
 */
int psp_decrypt(const uint8_t *key, const uint8_t *nonce,
                const uint8_t *ciphertext, uint16_t ct_len,
                const uint8_t *tag, uint8_t *plaintext);

/**
 * Get node address for this device
 */
uint16_t psp_get_self_addr(void);

/**
 * Set the AES-128 key for this node pair
 */
void psp_set_key(const uint8_t key[PSP_AES_KEY_SIZE]);

/**
 * Print frame for debugging
 */
void psp_print_frame(const psp_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* PSP_PROTOCOL_H */
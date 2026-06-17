/**
 * @file mesh_packet.h
 * @brief SoundNest Sub-GHz mesh network packet definitions.
 *
 * Defines the packet format, message types, and data structures used
 * for communication between all SoundNest nodes over the 868MHz LoRa mesh.
 */

#ifndef MESH_PACKET_H
#define MESH_PACKET_H

#include <stdint.h>
#include <stdbool.h>

/* ── Network Constants ─────────────────────────────────────────────── */

#define MESH_PREAMBLE_LEN       4
#define MESH_SYNC_WORD          0x4E53  /* "SN" */
#define MESH_MAX_PAYLOAD        64
#define MESH_MIC_LEN            4       /* AES-128-CCM tag */
#define MESH_HEADER_LEN        12
#define MESH_MAX_NODES          12
#define MESH_SUPERFRAME_MS      10000   /* 10-second TDMA superframe */
#define MESH_SLOT_MS            800     /* TDMA slot duration */
#define MESH_GUARD_MS           200     /* Guard time between slots */

/* ── Radio Configuration ───────────────────────────────────────────── */

typedef enum {
    RADIO_REGION_EU_868 = 0,
    RADIO_REGION_US_915 = 1,
    RADIO_REGION_AS_923 = 2,
    RADIO_REGION_AU_915 = 3,
} mesh_region_t;

typedef enum {
    RADIO_SF7BW125  = 0,  /* Urban, fastest   ~5kbps */
    RADIO_SF8BW125  = 1,  /* Mixed             ~3kbps */
    RADIO_SF9BW125  = 2,  /* Suburban          ~1.8kbps */
    RADIO_SF10BW125 = 3,  /* Long range        ~1kbps */
    RADIO_SF11BW125 = 4,  /* Very long range   ~0.5kbps */
    RADIO_SF12BW125 = 5,  /* Max range         ~0.3kbps */
} mesh_sf_t;

/* ── Node Types ────────────────────────────────────────────────────── */

typedef enum {
    NODE_TYPE_HUB              = 0x01,
    NODE_TYPE_ROOM_SENSOR      = 0x02,
    NODE_TYPE_MASKING_SPEAKER  = 0x03,
    NODE_TYPE_WEARABLE_TAG     = 0x04,
} mesh_node_type_t;

/* ── Message Types ─────────────────────────────────────────────────── */

typedef enum {
    MSG_TYPE_JOIN_REQ      = 0x01,  /* Node → Hub: request to join */
    MSG_TYPE_JOIN_ACK      = 0x02,  /* Hub → Node: join accepted */
    MSG_TYPE_JOIN_NACK     = 0x03,  /* Hub → Node: join rejected */
    MSG_TYPE_EVENT_REPORT  = 0x04,  /* Sensor → Hub: sound event detected */
    MSG_TYPE_SPL_REPORT    = 0x05,  /* Sensor → Hub: periodic SPL reading */
    MSG_TYPE_MASKING_CMD   = 0x06,  /* Hub → Speaker: masking command */
    MSG_TYPE_ALERT_CMD     = 0x07,  /* Hub → Wearable: haptic/LED alert */
    MSG_TYPE_CONFIG_UPDATE = 0x08,  /* Hub → Node: config change */
    MSG_TYPE_OTA_BLOCK     = 0x09,  /* Hub → Node: firmware block */
    MSG_TYPE_OTA_REQUEST   = 0x0A,  /* Node → Hub: request OTA block */
    MSG_TYPE_HEARTBEAT     = 0x0B,  /* Bidirectional: keep-alive */
    MSG_TYPE_DOSE_REPORT   = 0x0C,  /* Wearable → Hub: sound dose update */
    MSG_TYPE_DOSE_QUERY    = 0x0D,  /* Wearable → Hub: request dose calc */
    MSG_TYPE_MASKING_FEEDBACK = 0x0E, /* Speaker → Hub: masking status */
    MSG_TYPE_LOCALIZATION  = 0x0F,  /* Sensor → Hub: sound direction */
} mesh_msg_type_t;

/* ── Sound Event Classes ───────────────────────────────────────────── */

typedef enum {
    /* Alarm category (0x01-0x0F) */
    SOUND_SMOKE_ALARM      = 0x01,
    SOUND_CO_ALARM         = 0x02,
    SOUND_BURGLAR_ALARM    = 0x03,
    SOUND_CAR_ALARM        = 0x04,
    SOUND_TIMER_ALARM      = 0x05,

    /* Door category (0x10-0x1F) */
    SOUND_DOORBELL         = 0x10,
    SOUND_DOOR_KNOCK       = 0x11,
    SOUND_DOOR_OPEN        = 0x12,
    SOUND_DOOR_CLOSE       = 0x13,

    /* Human category (0x20-0x2F) */
    SOUND_SPEECH           = 0x20,
    SOUND_CRYING_BABY     = 0x21,
    SOUND_COUGH            = 0x22,
    SOUND_SNEEZE           = 0x23,
    SOUND_LAUGH            = 0x24,
    SOUND_SHOUT            = 0x25,

    /* Animal category (0x30-0x3F) */
    SOUND_DOG_BARK         = 0x30,
    SOUND_CAT_MEOW         = 0x31,
    SOUND_BIRD_CHIRP       = 0x32,

    /* Kitchen category (0x40-0x4F) */
    SOUND_MICROWAVE        = 0x40,
    SOUND_BLENDER          = 0x41,
    SOUND_DISHWASHER       = 0x42,
    SOUND_KETTLE           = 0x43,
    SOUND_FAUCET            = 0x44,

    /* Home category (0x50-0x5F) */
    SOUND_VACUUM           = 0x50,
    SOUND_WASHER           = 0x51,
    SOUND_DRYER            = 0x52,
    SOUND_FAN              = 0x53,
    SOUND_AC_UNIT          = 0x54,
    SOUND_TV               = 0x55,
    SOUND_MUSIC            = 0x56,

    /* Traffic category (0x60-0x6F) */
    SOUND_CAR_HORN         = 0x60,
    SOUND_SIREN            = 0x61,
    SOUND_ENGINE           = 0x62,
    SOUND_MOTORCYCLE       = 0x63,
    SOUND_BICYCLE_BELL     = 0x64,

    /* Nature category (0x70-0x7F) */
    SOUND_RAIN             = 0x70,
    SOUND_THUNDER          = 0x71,
    SOUND_WIND             = 0x72,
    SOUND_RUNNING_WATER    = 0x73,

    /* Work category (0x80-0x8F) */
    SOUND_PHONE_RING       = 0x80,
    SOUND_NOTIFICATION     = 0x81,
    SOUND_KEYBOARD         = 0x82,

    /* Alert category (0x90-0x9F) */
    SOUND_GLASS_BREAK      = 0x90,
    SOUND_CRASH            = 0x91,
    SOUND_GUNSHOT          = 0x92,

    /* Special */
    SOUND_UNKNOWN          = 0xFF,
    SOUND_SILENCE          = 0x00,
} sound_event_t;

/* ── Alert Priority ─────────────────────────────────────────────────── */

typedef enum {
    ALERT_PRIORITY_INFO      = 0,  /* LED only */
    ALERT_PRIORITY_LOW       = 1,  /* Single haptic pulse */
    ALERT_PRIORITY_MEDIUM   = 2,  /* Double haptic pulse + LED */
    ALERT_PRIORITY_HIGH     = 3,  /* Triple haptic pulse + LED + display */
    ALERT_PRIORITY_CRITICAL = 4,  /* Long haptic + LED flash + buzzer */
} alert_priority_t;

/* ── Masking Modes ─────────────────────────────────────────────────── */

typedef enum {
    MASKING_OFF            = 0x00,
    MASKING_WHITE_NOISE    = 0x01,
    MASKING_PINK_NOISE    = 0x02,
    MASKING_BROWN_NOISE   = 0x03,
    MASKING_NATURE_RAIN   = 0x04,
    MASKING_NATURE_STREAM = 0x05,
    MASKING_NATURE_FOREST = 0x06,
    MASKING_NATURE_OCEAN  = 0x07,
    MASKING_TINNITUS      = 0x08,
    MASKING_PRIVACY       = 0x09,
    MASKING_CUSTOM        = 0x0A,
} masking_mode_t;

/* ── Packet Structure ──────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t sync_word;      /* MESH_SYNC_WORD "SN" */
    uint8_t  length;         /* Total packet length (header + payload + MIC) */
    uint16_t src_addr;       /* Source node address */
    uint16_t dst_addr;       /* Destination (0xFFFF = broadcast) */
    uint8_t  msg_type;       /* mesh_msg_type_t */
    uint16_t seq_num;        /* Sequence number */
} mesh_header_t;

typedef struct __attribute__((packed)) {
    mesh_header_t header;
    uint8_t       payload[MESH_MAX_PAYLOAD];
    uint8_t       mic[MESH_MIC_LEN];
} mesh_packet_t;

/* ── Join Request Payload ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  node_type;       /* mesh_node_type_t */
    uint8_t  firmware_ver[4]; /* Semantic version (major.minor.patch.build) */
    uint32_t capabilities;    /* Bitfield of supported features */
    uint8_t  hw_revision;     /* Hardware revision number */
} join_req_payload_t;

#define CAPABILITY_SPL_METER      (1 << 0)
#define CAPABILITY_MIC_ARRAY      (1 << 1)
#define CAPABILITY_CLASSIFIER     (1 << 2)
#define CAPABILITY_LOCALIZER      (1 << 3)
#define CAPABILITY_MASKING        (1 << 4)
#define CAPABILITY_TINNITUS       (1 << 5)
#define CAPABILITY_PRIVACY        (1 << 6)
#define CAPABILITY_HAPTIC        (1 << 7)
#define CAPABILITY_LED           (1 << 8)
#define CAPABILITY_ACCELEROMETER (1 << 9)
#define CAPABILITY_TEMP_HUMID    (1 << 10)
#define CAPABILITY_LIGHT         (1 << 11)
#define CAPABILITY_PIR           (1 << 12)

/* ── Join ACK Payload ──────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t assigned_addr;   /* Assigned node address */
    uint8_t  tdma_slot;      /* Assigned TDMA slot number */
    uint8_t  sf_config;      /* Spreading factor configuration */
    uint8_t  channel;        /* Radio channel */
    uint8_t  encryption_key[16]; /* AES-128 session key */
} join_ack_payload_t;

/* ── Sound Event Report Payload ────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  sound_class;    /* sound_event_t */
    uint8_t  confidence;     /* 0-100% */
    int8_t   direction_deg;  /* Sound source direction (-90 to +90) */
    uint8_t  spl_dba;        /* A-weighted SPL (0-255 dB) */
    uint8_t  spl_dbc;        /* C-weighted SPL */
    uint8_t  spl_dbz;        /* Z-weighted SPL */
    uint8_t  peak_spl;       /* Peak SPL during event */
    uint16_t duration_ms;    /* Event duration in ms */
    uint32_t timestamp;      /* Unix timestamp */
    uint8_t  room_id;        /* Room identifier */
    uint8_t  occupancy;      /* PIR occupancy flag */
    float    temp_c;         /* Temperature at detection */
    uint8_t  humidity_pct;   /* Humidity at detection */
} event_report_payload_t;

/* ── SPL Report Payload ───────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  spl_dba;        /* Current A-weighted SPL */
    uint8_t  spl_dbc;        /* Current C-weighted SPL */
    uint8_t  spl_dbz;        /* Current Z-weighted SPL */
    uint8_t  spl_min;        /* Min SPL in window */
    uint8_t  spl_max;        /* Max SPL in window */
    uint8_t  spl_eq;         /* Equivalent level Leq */
    uint8_t  spectral_32[32]; /* 1/3-octave spectrum (dB) */
    uint8_t  active_events;  /* Bitfield of active sound classes */
    uint8_t  occupancy;      /* PIR occupancy */
    float    temp_c;
    uint8_t  humidity_pct;
    uint32_t timestamp;
    uint16_t battery_mv;    /* Battery voltage in mV */
} spl_report_payload_t;

/* ── Masking Command Payload ────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  mode;           /* masking_mode_t */
    uint8_t  volume;         /* 0-100% */
    uint8_t  stereo_balance; /* 0-100 (50=center) */
    uint16_t freq_hz[2];    /* Tinnitus: center freq range */
    uint8_t  bandwidth;     /* Tinnitus: bandwidth index */
    uint8_t  fade_in_ms;    /* Fade in duration (ms / 100) */
    uint8_t  fade_out_ms;   /* Fade out duration (ms / 100) */
    uint8_t  duration_min;  /* Auto-stop after N minutes (0=forever) */
    uint8_t  adaptive;      /* 1=adaptive volume, 0=fixed */
    uint8_t  reserved[4];   /* Future use */
} masking_cmd_payload_t;

/* ── Alert Command Payload ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  priority;      /* alert_priority_t */
    uint8_t  sound_class;   /* sound_event_t that triggered alert */
    uint8_t  haptic_pattern; /* Haptic vibration pattern (0-15) */
    uint8_t  led_color;     /* LED color (R:3|G:3|B:2) */
    uint8_t  led_pattern;   /* LED blink pattern (0-7) */
    uint8_t  spl_dba;       /* SPL of triggering event */
    uint8_t  direction;    /* Direction of sound */
    uint32_t timestamp;     /* Event timestamp */
    uint8_t  message_len;  /* Length of text message */
    char     message[16];  /* Short text description */
} alert_cmd_payload_t;

/* ── Sound Dose Report Payload ──────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  daily_dose_pct;   /* Daily dose percentage (0-255%) */
    uint8_t  current_spl_dba;  /* Current SPL */
    uint16_t twa_dba_x10;     /* Time-weighted average (dBA × 10) */
    uint8_t  peak_dba;        /* Peak SPL today */
    uint16_t exposure_min;   /* Total exposure minutes today */
    uint8_t  activity;        /* Current activity (0=still, 1=walk, 2=run) */
    uint16_t battery_mv;     /* Battery voltage mV */
    uint32_t timestamp;
} dose_report_payload_t;

/* ── Config Update Payload ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  config_id;      /* Configuration parameter ID */
    uint8_t  config_len;     /* Length of config data */
    uint8_t  config_data[56]; /* Configuration data */
} config_update_payload_t;

/* Config IDs */
#define CONFIG_ID_MEASUREMENT_INTERVAL  0x01
#define CONFIG_ID_ALERT_THRESHOLDS      0x02
#define CONFIG_ID_MASKING_PROFILE       0x03
#define CONFIG_ID_ROOM_ASSIGNMENT       0x04
#define CONFIG_ID_TINNITUS_PROFILE      0x05
#define CONFIG_ID_SLEEP_SCHEDULE        0x06
#define CONFIG_ID_DOSIMETRY_PARAMS      0x07
#define CONFIG_ID_PRIVACY_SETTINGS      0x08

/* ── Heartbeat Payload ──────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  node_type;
    uint16_t battery_mv;
    uint8_t  rssi;         /* Signal strength */
    uint8_t  status;       /* Bitfield: bit0=mic, bit1=radio, bit2=sensor */
    uint32_t uptime_sec;
    uint16_t events_today;
    uint16_t packets_sent;
    uint16_t packets_missed;
} heartbeat_payload_t;

/* ── OTA Block Payload ──────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t firmware_size;     /* Total firmware size */
    uint32_t block_offset;     /* Offset of this block */
    uint8_t  block_size;       /* Size of data in this block (max 48) */
    uint8_t  block_data[48];  /* Firmware data */
    uint16_t crc16;            /* CRC16 of block_data */
    uint8_t  total_blocks;    /* Total number of blocks */
    uint8_t  block_index;     /* This block's index */
} ota_block_payload_t;

/* ── Localization Payload ───────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    int8_t   azimuth_deg;    /* Horizontal angle (-90 to +90) */
    int8_t   elevation_deg;  /* Vertical angle (-45 to +45) */
    uint8_t  confidence;     /* Localization confidence (0-100) */
    uint8_t  source_class;   /* sound_event_t of localized source */
    uint8_t  spl_dba;       /* SPL at arrival */
    uint32_t timestamp;
} localization_payload_t;

/* ── Mesh Statistics ────────────────────────────────────────────────── */

typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t tx_errors;
    uint32_t rx_errors;
    uint32_t tx_retries;
    uint32_t crc_failures;
    int8_t   last_rssi;
    uint8_t  last_snr;
    uint32_t uptime_sec;
} mesh_stats_t;

#endif /* MESH_PACKET_H */
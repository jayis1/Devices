/*
 * mesh_protocol.h — Shared Sub-GHz mesh protocol definitions
 *
 * Used by all PorchGuard nodes (hub, porch-camera, mailbox, lock)
 * TDMA: 5 slots × 100ms = 500ms frame
 *
 * SAFETY NOTE: Pirate/tamper alerts override normal TDMA on SF12 (max range).
 */

#pragma once

#include <stdint.h>

/* ---- Node IDs ---- */
#define NODE_ID_HUB        0x00
#define NODE_ID_BROADCAST  0xFF
#define NODE_ID_CAMERA     0x01
#define NODE_ID_MAILBOX    0x02
#define NODE_ID_LOCK       0x03

/* ---- TDMA timing ---- */
#define TDMA_FRAME_MS       500
#define TDMA_SLOT_MS        100
#define TDMA_NUM_SLOTS       5
#define TDMA_GUARD_US       500   /* guard time between slots */

/* ---- Packet types ---- */
#define PKT_CAMERA_DATA     0x01
#define PKT_MAILBOX_DATA    0x02
#define PKT_LOCK_DATA       0x03
#define PKT_COMMAND         0x04
#define PKT_ACK             0x05
#define PKT_OTA_BLOCK       0x06
#define PKT_CALIBRATION     0x07
#define PKT_PIRATE_ALERT    0x08  /* highest priority — SF12 broadcast */
#define PKT_TAMPER_ALERT    0x09
#define PKT_DELIVERY_EVENT  0x0A
#define PKT_HEARTBEAT       0x0B
#define PKT_CLIP_REF        0x0C

/* ---- Sync word ---- */
#define MESH_SYNC_WORD      0x5047  /* "PG" */

/* Max payload size (fits in LoRa SF7 125kHz) */
#define MESH_MAX_PAYLOAD    50

/* ---- Packet structure (over the air) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[4];   /* 0xAA 0xAA 0xAA 0xAA */
    uint16_t sync;          /* MESH_SYNC_WORD */
    uint8_t  len;           /* payload length */
    uint8_t  src_id;        /* source node ID */
    uint8_t  dst_id;        /* destination node ID (0xFF=broadcast) */
    uint8_t  pkt_type;      /* PKT_* type */
    uint8_t  payload[MESH_MAX_PAYLOAD];
    uint16_t crc16;         /* CRC-16/CCITT over len+src+dst+type+payload */
} mesh_packet_t;

/* ---- Camera data payload (32 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  presence_state;   /* 0=clear,1=person,2=parcel-only,3=person+parcel */
    uint8_t  person_id;        /* 0=none,1=resident,2=courier,3=unknown,4=unknown-loitering */
    uint8_t  person_conf;      /* 0-255 re-ID confidence */
    uint8_t  parcel_class;     /* 0=none,1=small,2=medium,3=large,4=envelope */
    uint8_t  parcel_conf;      /* 0-255 */
    uint8_t  pirate_risk;      /* 0-255 (0.0-1.0) ML risk */
    int16_t  mmwave_dist_cm;   /* distance to nearest presence cm (-1=none) */
    int16_t  ambient_temp_c_x10; /* ambient temp ×10 °C */
    uint8_t  armed;            /* 0=disarmed,1=armed */
    uint8_t  siren_active;     /* 0=off,1=on (hub-controlled) */
    uint8_t  tamper_flag;      /* 0=ok,1=cover-moved,2=tilt */
    uint8_t  knock_detected;   /* 0=no,1=knock,2=glass-break */
    uint8_t  clip_ready;       /* 0=no,1=clip on SD, ref in clip_id */
    uint16_t clip_id;          /* MicroSD clip index */
    uint8_t  battery_pct;     /* 0-100 (supcap charge for camera) */
    uint8_t  signal_rssi;      /* RSSI dB above -100 */
    uint8_t  wifi_up;          /* 0=down,1=up */
    uint8_t  power_lost;       /* 0=ok,1=power lost (supcap mode) */
    uint16_t reserved;         /* future use */
} camera_data_payload_t;

/* ---- Mailbox data payload (24 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  door_state;     /* 0=closed,1=open */
    uint8_t  mail_class;      /* 0=empty,1=letter,2=thick,3=parcel */
    uint16_t weight_mg;      /* mail weight mg */
    int16_t  temp_c_x10;      /* mailbox temp ×10 °C */
    uint16_t light_lux;      /* ambient light lux */
    uint8_t  tamper_flag;    /* 0=ok,1=tilt,2=forced-open */
    uint8_t  battery_pct;   /* 0-100 */
    uint8_t  solar_mv;      /* solar panel voltage ×10 mV */
    uint8_t  signal_rssi;   /* RSSI */
    uint8_t  last_event;    /* 0=none,1=mail-arrived,2=mail-collected,3=tamper */
    uint16_t event_age_s;  /* seconds since last event */
    uint16_t reserved;
    uint8_t  reserved2[5];
} mailbox_data_payload_t;

/* ---- Lock data payload (20 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  lock_state;     /* 0=locked,1=unlocked,2=error */
    uint8_t  door_state;     /* 0=closed,1=open */
    uint8_t  last_unlock_src;/* 0=app,1=keypad,2=auto,3=courier-code,4=forced */
    uint8_t  last_code_id;   /* code slot id used (0 if none) */
    uint8_t  tamper_flag;    /* 0=ok,1=tilt,2=motor-anomaly */
    uint8_t  battery_pct;   /* 0-100 */
    uint8_t  auto_lock_enabled; /* 0=off,1=on */
    uint16_t door_open_s;    /* seconds door has been open */
    uint8_t  garage_relay_on;/* 0=off,1=on */
    uint8_t  codes_active;  /* count of active one-time codes */
    uint8_t  keypad_active;  /* 0=idle,1=key-in-progress */
    uint8_t  signal_rssi;
    uint8_t  reserved[5];
} lock_data_payload_t;

/* ---- Pirate alert payload (16 bytes) — highest priority ---- */
typedef struct __attribute__((packed)) {
    uint8_t  alert_level;       /* 3=critical, 4=emergency */
    uint8_t  pirate_risk;       /* 0-255 */
    uint8_t  person_id;         /* who triggered (3=unknown,4=loitering) */
    uint8_t  parcel_class;      /* parcel that was grabbed */
    uint8_t  has_clip;          /* 0=no,1=yes (clip_id valid) */
    uint16_t clip_id;           /* MicroSD clip index */
    int16_t  mmwave_dist_cm;    /* last known distance */
    uint8_t  siren_requested;   /* 0=no,1=yes */
    uint8_t  source_node;       /* usually camera */
    uint16_t timestamp_ms;     /* hub-relative */
    uint16_t reserved;
} pirate_alert_payload_t;

/* ---- Tamper alert payload (14 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  alert_level;
    uint8_t  tamper_type;       /* 0=cover-moved,1=tilt,2=forced-entry,3=power-cut,4=fishing */
    uint8_t  source_node;       /* camera/mailbox/lock */
    uint8_t  severity;         /* 1=warning,2=critical,3=emergency */
    uint16_t timestamp_ms;
    uint8_t  reserved[8];
} tamper_alert_payload_t;

/* ---- Delivery event payload (18 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  event_type;     /* 0=parcel-drop,1=mail-arrived,2=mail-collected,3=parcel-collected-by-resident */
    uint8_t  parcel_class;   /* 0=none,1=small,2=medium,3=large,4=envelope,5=letter,6=thick */
    uint8_t  courier_id;     /* 0=unknown,1=UPS,2=FedEx,3=USPS,4=Amazon,5=DHL,6=other-known */
    uint8_t  source_node;    /* camera or mailbox */
    uint8_t  has_clip;       /* 0=no,1=yes */
    uint16_t clip_id;        /* MicroSD clip index */
    int16_t  temp_c_x10;     /* ambient temp at drop */
    uint8_t  weight_mg_hi;   /* mailbox weight (if from mailbox) — high byte */
    uint16_t reserved;
} delivery_event_payload_t;

/* ---- Clip reference payload (12 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t clip_id;       /* unique clip id */
    uint8_t  event_type;    /* event that triggered clip */
    uint8_t  node_source;   /* which node recorded */
    uint8_t  duration_s;    /* clip length seconds */
    uint8_t  uploaded;      /* 0=sd-only,1=cloud-uploaded */
    uint16_t timestamp_s;  /* event time (hub-relative) */
    uint32_t reserved;
} clip_ref_payload_t;

/* ---- Command payload (variable) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;    /* CMD_* */
    uint8_t  param_len;   /* length of following params */
    uint8_t  params[16];  /* command-specific parameters */
} command_payload_t;

/* ---- Command types ---- */
#define CMD_UNLOCK          0x01  /* params: [source(1), code_id(1)] */
#define CMD_LOCK            0x02  /* params: none */
#define CMD_GARAGE_RELAY    0x03  /* params: [duration_s(1)] */
#define CMD_ISSUE_CODE      0x04  /* params: [code_id(1), window_minutes(1), code_digits(6)] */
#define CMD_REVOKE_CODE     0x05  /* params: [code_id(1)] */
#define CMD_SIREN_ON        0x06  /* params: [duration_s(1)] */
#define CMD_SIREN_OFF       0x07  /* params: none */
#define CMD_ARM             0x08  /* params: [armed(1)] */
#define CMD_CALIBRATE       0x09  /* params: [sensor_id(1), value(4)] */
#define CMD_CAPTURE_CLIP    0x0A  /* params: [duration_s(1)] */
#define CMD_AUTO_LOCK       0x0B  /* params: [enabled(1), delay_s(1)] */
#define CMD_PROXIMITY_UNLOCK 0x0C /* params: [resident_id(1)] */

/* ---- Alert levels ---- */
#define ALERT_OK        0
#define ALERT_INFO      1
#define ALERT_WARNING   2
#define ALERT_CRITICAL  3
#define ALERT_EMERGENCY 4

/* ---- Presence / person states ---- */
#define PRESENCE_CLEAR       0
#define PRESENCE_PERSON      1
#define PRESENCE_PARCEL_ONLY 2
#define PRESENCE_BOTH        3

#define PERSON_NONE          0
#define PERSON_RESIDENT      1
#define PERSON_COURIER       2
#define PERSON_UNKNOWN       3
#define PERSON_LOITERING     4

/* ---- Parcel classes ---- */
#define PARCEL_NONE      0
#define PARCEL_SMALL     1
#define PARCEL_MEDIUM    2
#define PARCEL_LARGE     3
#define PARCEL_ENVELOPE  4

/* ---- Mail classes ---- */
#define MAIL_EMPTY     0
#define MAIL_LETTER    1
#define MAIL_THICK     2
#define MAIL_PARCEL    3

/* ---- Courier IDs ---- */
#define COURIER_UNKNOWN 0
#define COURIER_UPS     1
#define COURIER_FEDEX   2
#define COURIER_USPS    3
#define COURIER_AMAZON  4
#define COURIER_DHL     5
#define COURIER_OTHER   6

/* ---- Unlock sources ---- */
#define UNLOCK_APP      0
#define UNLOCK_KEYPAD   1
#define UNLOCK_AUTO     2
#define UNLOCK_COURIER  3
#define UNLOCK_FORCED   4

/* ---- CRC-16/CCITT calculation ---- */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len);

/* Build a mesh packet */
uint16_t mesh_build_packet(uint8_t src, uint8_t dst, uint8_t type,
                            const uint8_t *payload, uint8_t payload_len,
                            mesh_packet_t *out);

/* Parse and validate a received packet */
int8_t mesh_parse_packet(const uint8_t *raw, uint16_t raw_len, mesh_packet_t *out);
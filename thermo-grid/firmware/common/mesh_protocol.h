/*
 * mesh_protocol.h — Shared Sub-GHz mesh protocol definitions
 *
 * Used by all ThermoGrid nodes (hub, room-sensor, zone-actuator, comfort-tag)
 * TDMA: dynamic frame (N sensors + 2) × 100ms
 *
 * SAFETY NOTE: Freeze/window alerts override normal TDMA on SF12 (max range).
 */

#pragma once

#include <stdint.h>

/* ---- Node IDs ---- */
#define NODE_ID_HUB           0x00
#define NODE_ID_BROADCAST     0xFF
/* Sensor/actuator IDs are assigned dynamically at enrollment (0x10-0x7F) */
/* Comfort tag IDs are 0x80-0x8F (paired persons) */

/* ---- TDMA timing ---- */
#define TDMA_FRAME_MS_BASE   200   /* hub + ctrl = 2 slots */
#define TDMA_SLOT_MS          100
#define TDMA_GUARD_US        500
#define TDMA_MAX_NODES        32    /* max sensors + actuators */

/* ---- Packet types ---- */
#define PKT_SENSOR_DATA      0x01
#define PKT_ACTUATOR_DATA    0x02
#define PKT_COMFORT_DATA     0x03
#define PKT_COMMAND          0x04
#define PKT_ACK              0x05
#define PKT_OTA_BLOCK        0x06
#define PKT_CALIBRATION      0x07
#define PKT_FREEZE_ALERT     0x08  /* highest priority — SF12 broadcast */
#define PKT_WINDOW_OPEN      0x09
#define PKT_ZONE_SETPOINT    0x0A
#define PKT_HEARTBEAT        0x0B
#define PKT_ENERGY_REPORT    0x0C
#define PKT_SOLAR_STATUS     0x0D
#define PKT_TOU_SCHEDULE     0x0E
#define PKT_COMFORT_VOTE    0x0F

/* ---- Sync word ---- */
#define MESH_SYNC_WORD      0x5447  /* "TG" */

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

/* ---- Sensor data payload (36 bytes) — room sensor uplink ---- */
typedef struct __attribute__((packed)) {
    int16_t  air_temp_cx100;    /* air temperature ×100 °C (SHT45) */
    int16_t  mrt_cx100;         /* mean radiant temperature ×100 °C (MLX90640) */
    uint16_t humidity_centi;    /* relative humidity ×100 % (SHT45) */
    int16_t  air_vel_cms_x100;  /* air velocity ×100 cm/s (SDP810) */
    int16_t  pressure_pa;       /* barometric pressure Pa (BMP390) - truncated */
    uint8_t  occupancy;         /* 0=empty,1=person,2=pet,3=multi-person */
    uint8_t  occupancy_conf;    /* 0-255 confidence */
    uint16_t light_lux;         /* ambient light lux (ALS-PT19) */
    uint16_t co2_ppm;           /* CO2 ppm (SCD41, 0 if no sensor) */
    int8_t   window_state;      /* 0=closed,1=open(detected),-1=unknown */
    uint8_t  solar_gain_w;      /* estimated solar heat input watts (0-255) */
    uint8_t  battery_pct;       /* 0-100 */
    uint8_t  solar_mv;          /* solar panel voltage ×10 mV */
    int8_t   signal_rssi;       /* RSSI dBm (signed) */
    uint8_t  fault_flags;       /* bit0=sensor,bit1=radio,bit2=battery,bit3=cal */
    uint16_t seq_num;           /* sequence number for dedup */
    uint8_t  reserved[2];
} sensor_data_payload_t;

/* ---- Actuator data payload (28 bytes) — zone actuator uplink ---- */
typedef struct __attribute__((packed)) {
    uint8_t  valve_pos;          /* 0-100% valve/damper open */
    uint8_t  valve_target;       /* 0-100% commanded target */
    int16_t  pipe_temp_cx100;    /* pipe/floor temp ×100 °C (DS18B20) */
    uint16_t flow_mlmin;         /* water flow ml/min (YF-S201, 0 if none) */
    uint16_t energy_btu_x10;     /* accumulated energy BTU ×10 (computed) */
    uint8_t  zone_mode;          /* 0=off,1=heating,2=cooling,3=frost,4=solar-boost */
    uint8_t  relay_state;        /* bitmask: bit0=relay1,bit1=relay2 */
    uint8_t  fault_flags;       /* bit0=motor,bit1=comm,bit2=valve-stuck,bit3=overtemp */
    uint8_t  battery_pct;       /* 0-100 (or 255 if wired power) */
    uint8_t  power_source;     /* 0=24VAC wired,1=AA battery */
    int8_t   signal_rssi;
    uint16_t pipe_target_cx100; /* commanded pipe/floor temp ×100 °C */
    uint8_t  zone_id;           /* which zone this actuator controls */
    uint8_t  pid_active;        /* 0=idle,1=PID running */
    uint16_t seq_num;
    uint8_t  reserved[2];
} actuator_data_payload_t;

/* ---- Comfort data payload (24 bytes) — comfort tag uplink ---- */
typedef struct __attribute__((packed)) {
    int16_t  skin_temp_cx100;    /* skin/wrist temp ×100 °C (MAX30208) */
    int16_t  air_temp_cx100;    /* ambient near body ×100 °C (TMP117) */
    uint16_t humidity_centi;     /* local humidity ×100 % (SHT40) */
    uint8_t  hr_bpm;            /* heart rate bpm (MAX30101) */
    uint8_t  hrv_ms;            /* HRV RMSSD ms (0 if unavailable) */
    uint8_t  activity;          /* 0=sedentary,1=light,2=moderate,3=vigorous,4=sleeping */
    uint8_t  activity_conf;     /* 0-255 */
    int8_t   comfort_score;     /* -3 cold .. 0 neutral .. +3 hot (predicted) */
    uint8_t  comfort_conf;      /* 0-255 */
    uint8_t  person_id;         /* which person (0x80-0x8F) */
    uint8_t  vote_pending;      /* 0=none,1="I'm cold",2="I'm hot" (button press) */
    uint8_t  battery_pct;       /* 0-100 */
    int8_t   signal_rssi;       /* BLE RSSI (signed) */
    uint16_t seq_num;
    uint8_t  reserved[3];
} comfort_data_payload_t;

/* ---- Freeze alert payload (16 bytes) — highest priority, SF12 ---- */
typedef struct __attribute__((packed)) {
    uint8_t  alert_level;       /* 3=critical, 4=emergency */
    uint8_t  room_id;           /* which room sensor detected */
    int16_t  temp_cx100;        /* current temp ×100 °C */
    int16_t  mrt_cx100;         /* mean radiant temp ×100 °C */
    uint8_t  all_valves_open;   /* 1 if hub has commanded all open */
    uint8_t  boiler_relay_on;   /* 1 if boiler/heat-pump forced on */
    uint16_t timestamp_s;       /* hub-relative seconds */
    uint8_t  reserved[6];
} freeze_alert_payload_t;

/* ---- Window open alert payload (12 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  room_id;
    uint8_t  confidence;        /* 0-255 */
    int16_t  temp_drop_cx100;    /* temperature drop ×100 °C in 60s */
    int16_t  air_vel_cms_x100;  /* air velocity at detection ×100 cm/s */
    uint16_t duration_s;        /* how long window has been open (updates) */
    uint8_t  reserved[2];
} window_open_payload_t;

/* ---- Zone setpoint payload (16 bytes) — hub → actuator ---- */
typedef struct __attribute__((packed)) {
    uint8_t  zone_id;           /* which zone */
    int16_t  setpoint_cx100;     /* target room temp ×100 °C */
    int16_t  pipe_target_cx100;  /* target pipe/floor temp ×100 °C */
    uint8_t  mode;              /* 0=off,1=heating,2=cooling,3=frost,4=solar-boost */
    uint8_t  valve_pos_override;/* 255=no override, 0-100=force position */
    uint16_t boost_minutes;     /* temporary boost duration (0=none) */
    uint8_t  source;            /* 0=schedule,1=manual,2=solar,3=comfort,4=frost */
    uint8_t  comfort_person;    /* person_id for comfort-based control (0xFF=none) */
    uint8_t  reserved[3];
} zone_setpoint_payload_t;

/* ---- Energy report payload (20 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  zone_id;
    uint32_t energy_wh_x10;     /* accumulated Wh ×10 since last report */
    uint16_t flow_total_l;      /* total water flow liters (hydronic) */
    uint16_t uptime_minutes;    /* actuator uptime since last report */
    int16_t  avg_pipe_temp_cx100; /* average pipe temp ×100 °C */
    int16_t  avg_room_temp_cx100; /* average room temp ×100 °C (from sensor) */
    uint8_t  cost_cents;        /* estimated cost in cents ×10 (if TOU known) */
    uint8_t  tariff_period;     /* 0=off-peak,1=mid-peak,2=peak,3=solar */
    uint8_t  reserved[3];
} energy_report_payload_t;

/* ---- Solar status payload (12 bytes) — hub → actuators ---- */
typedef struct __attribute__((packed)) {
    int16_t  production_w;       /* current solar production watts */
    int16_t  base_load_w;        /* home base load watts */
    int16_t  surplus_w;          /* production - base load (available for boost) */
    uint8_t  boost_recommended;  /* 0=no,1=yes (surplus > threshold) */
    uint8_t  boost_target_c;     /* suggested boost temp (setpoint + N) */
    uint8_t  reserved[4];
} solar_status_payload_t;

/* ---- TOU schedule payload (10 bytes) — hub → actuators ---- */
typedef struct __attribute__((packed)) {
    uint8_t  current_period;    /* 0=off-peak,1=mid-peak,2=peak,3=solar */
    uint16_t rate_cents_x10;    /* current rate cents/kWh ×10 */
    uint16_t next_change_min;  /* minutes until next tariff period change */
    uint8_t  next_period;      /* what the next period will be */
    uint16_t next_rate_cents_x10;
    uint8_t  reserved[2];
} tou_schedule_payload_t;

/* ---- Comfort vote payload (8 bytes) — from tag ---- */
typedef struct __attribute__((packed)) {
    uint8_t  person_id;         /* 0x80-0x8F */
    int8_t   vote;              /* -3 cold .. 0 ok .. +3 hot */
    int16_t  skin_temp_cx100;
    uint8_t  activity;
    uint8_t  room_id;           /* which room person is in (from BLE RSSI or hub) */
    uint8_t  reserved[1];
} comfort_vote_payload_t;

/* ---- Command payload (variable) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;    /* CMD_* */
    uint8_t  param_len;   /* length of following params */
    uint8_t  params[16];  /* command-specific parameters */
} command_payload_t;

/* ---- Command types ---- */
#define CMD_SET_SETPOINT    0x01  /* params: [zone_id(1), setpoint_cx100(2), mode(1)] */
#define CMD_SET_VALVE       0x02  /* params: [zone_id(1), valve_pos(1)] */
#define CMD_SET_MODE        0x03  /* params: [zone_id(1), mode(1)] */
#define CMD_BOOST           0x04  /* params: [zone_id(1), boost_cx100(2), minutes(1)] */
#define CMD_FROST_PROTECT   0x05  /* params: [zone_id(1), enable(1)] */
#define CMD_SOLAR_BOOST     0x06  /* params: [zone_id(1), target_cx100(2), minutes(1)] */
#define CMD_CALIBRATE       0x07  /* params: [sensor_id(1), value(4)] */
#define CMD_ENROLL_SENSOR   0x08  /* params: [node_id(1), room_name(16)] */
#define CMD_REMOVE_NODE     0x09  /* params: [node_id(1)] */
#define CMD_ENROLL_TAG      0x0A  /* params: [person_id(1), name(16)] */
#define CMD_WINDOW_CLOSED   0x0B  /* params: [room_id(1)] */
#define CMD_SCHEDULE_UPDATE 0x0C  /* params: [zone_id(1), slot(1), setpoint_cx100(2)] */

/* ---- Zone modes ---- */
#define MODE_OFF           0
#define MODE_HEATING       1
#define MODE_COOLING       2
#define MODE_FROST         3
#define MODE_SOLAR_BOOST   4

/* ---- Occupancy states ---- */
#define OCC_EMPTY           0
#define OCC_PERSON          1
#define OCC_PET             2
#define OCC_MULTI           3

/* ---- Activity levels ---- */
#define ACT_SEDENTARY       0
#define ACT_LIGHT           1
#define ACT_MODERATE        2
#define ACT_VIGOROUS        3
#define ACT_SLEEPING        4

/* ---- Comfort vote scale ---- */
#define COMF_VERY_COLD     -3
#define COMF_COOL          -2
#define COMF_SLIGHTLY_COOL -1
#define COMF_NEUTRAL        0
#define COMF_SLIGHTLY_WARM  1
#define COMF_WARM           2
#define COMF_VERY_HOT       3

/* ---- Alert levels ---- */
#define ALERT_OK        0
#define ALERT_INFO      1
#define ALERT_WARNING   2
#define ALERT_CRITICAL  3
#define ALERT_EMERGENCY 4

/* ---- Fault flags ---- */
#define FAULT_SENSOR     0x01
#define FAULT_RADIO      0x02
#define FAULT_BATTERY    0x04
#define FAULT_CAL        0x08
#define FAULT_MOTOR      0x01
#define FAULT_COMM      0x02
#define FAULT_VALVE_STUCK 0x04
#define FAULT_OVERTEMP  0x08

/* ---- CRC-16/CCITT calculation ---- */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len);

/* Build a mesh packet */
uint16_t mesh_build_packet(uint8_t src, uint8_t dst, uint8_t type,
                            const uint8_t *payload, uint8_t payload_len,
                            mesh_packet_t *out);

/* Parse and validate a received packet */
int8_t mesh_parse_packet(const uint8_t *raw, uint16_t raw_len, mesh_packet_t *out);
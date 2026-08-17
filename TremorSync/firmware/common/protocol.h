#ifndef TREMORSYNC_PROTOCOL_H
#define TREMORSYNC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Node IDs */
#define NODE_ID_HUB            0x0001
#define NODE_ID_TREMOR_BAND    0x0002
#define NODE_ID_GAIT_POD       0x0003
#define NODE_ID_VOICE_NODE     0x0004
#define NODE_ID_MED_STATION    0x0005
#define NODE_ID_BROADCAST      0xFFFF

/* Message types */
#define MSG_TYPE_BEACON        0x01
#define MSG_TYPE_SENSOR_DATA   0x02
#define MSG_TYPE_FOG_WARNING   0x03
#define MSG_TYPE_MED_REMINDER  0x04
#define MSG_TYPE_JOIN_REQ      0x05
#define MSG_TYPE_JOIN_ACK      0x06
#define MSG_TYPE_HEARTBEAT     0x07
#define MSG_TYPE_OTA_CHUNK     0x08
#define MSG_TYPE_CONFIG        0x09
#define MSG_TYPE_CAL_REQ       0x0A
#define MSG_TYPE_ONOFF_STATE   0x0B
#define MSG_TYPE_FALL_ALERT    0x0C

/* Frame constants */
#define FRAME_PREAMBLE_LEN  4
#define FRAME_SYNC_LEN      2
#define FRAME_HEADER_LEN    12
#define FRAME_PAYLOAD_MAX   32
#define FRAME_CRC_LEN       2
#define FRAME_MAX_LEN       (FRAME_HEADER_LEN + FRAME_PAYLOAD_MAX + FRAME_CRC_LEN)

#define SYNC_WORD_0 0x2D
#define SYNC_WORD_1 0xD4

/* Haptic / cueing patterns */
#define HAPTIC_NONE           0x00
#define HAPTIC_SINGLE_TAP     0x01   /* med reminder */
#define HAPTIC_DOUBLE_PULSE   0x02   /* FOG warning */
#define HAPTIC_TRIPLE_BURST   0x03   /* fall alert */
#define HAPTIC_METRONOME_60   0x04   /* FOG cueing 60 BPM */
#define HAPTIC_METRONOME_80   0x05   /* FOG cueing 80 BPM */
#define HAPTIC_METRONOME_100  0x06   /* FOG cueing 100 BPM */
#define HAPTIC_METRONOME_120  0x07   /* FOG cueing 120 BPM */
#define HAPTIC_LONG_BUZZ      0x08

/* Tremor classes (TremorNet) */
#define TREMOR_NONE        0
#define TREMOR_RESTING     1
#define TREMOR_POSTURAL    2
#define TREMOR_ACTION      3
#define TREMOR_COUNT       4

/* ON/OFF states */
#define ONOFF_OFF          0
#define ONOFF_ON           1
#define ONOFF_TRANSITION   2

/* Speech classes (SpeechNet) */
#define SPEECH_NORMAL           0
#define SPEECH_MILD_HYPOPHONIA  1
#define SPEECH_MOD_HYPOPHONIA   2
#define SPEECH_SEVERE_HYPOPHONIA 3
#define SPEECH_DYSARTHRIC       4
#define SPEECH_COUNT            5

/* Frame structure */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[FRAME_PREAMBLE_LEN];
    uint8_t  sync[FRAME_SYNC_LEN];
    uint8_t  length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[FRAME_PAYLOAD_MAX];
    uint16_t crc;
} mesh_frame_t;

/* Sensor data payload variants (32 bytes max) */
typedef struct __attribute__((packed)) {
    uint8_t  tremor_class;      /* TREMOR_* */
    float    tremor_amplitude;  /* m/s² RMS in 4-6 Hz band */
    float    bradykinesia_idx;  /* 0-100 */
    uint8_t  onoff_state;       /* ONOFF_* */
    uint8_t  hr;                /* bpm */
    uint8_t  battery_pct;       /* 0-100 */
    uint8_t  reserved[14];
} tremor_payload_t;

typedef struct __attribute__((packed)) {
    float    stride_length;     /* m */
    float    cadence;           /* steps/min */
    float    freeze_index;      /* 0-1 */
    uint8_t  fog_detected;      /* bool */
    uint8_t  festination;       /* bool */
    uint8_t  battery_pct;
    uint8_t  reserved[15];
} gait_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  speech_class;      /* SPEECH_* */
    float    hypophonia_score;  /* 0-100 */
    float    f0_mean;           /* Hz */
    float    f0_std;            /* Hz */
    uint8_t  swallow_event;     /* 0=none, 1=normal, 2=prolonged, 3=cough */
    uint8_t  battery_pct;
    uint8_t  reserved[12];
} voice_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  dose_taken;        /* bool */
    uint8_t  pill_weight_mg;    /* verified weight */
    uint16_t minutes_since_dose;
    uint8_t  onoff_state;
    uint8_t  battery_pct;
    uint8_t  reserved[24];
} med_payload_t;

/* Function prototypes */
uint16_t protocol_crc16(const uint8_t *data, size_t len);
void protocol_build_frame(mesh_frame_t *frame, uint16_t src, uint16_t dst,
                           uint8_t msg_type, uint16_t seq,
                           const uint8_t *payload, uint8_t payload_len);
bool protocol_parse_frame(const uint8_t *raw, size_t len, mesh_frame_t *out);
bool protocol_validate_crc(const mesh_frame_t *frame);

#endif /* TREMORSYNC_PROTOCOL_H */
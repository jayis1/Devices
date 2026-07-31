/*
 * BloomSync — Protocol Header
 * Binary message encoding/decoding for BLE WAN (wide-area network).
 */
#ifndef BLOOMSYNC_PROTOCOL_H
#define BLOOMSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define BS_SYNC0           0x42  /* 'B' */
#define BS_SYNC1           0x53  /* 'S' */
#define BS_BROADCAST       0xFF
#define BS_MAX_PAYLOAD     240
#define BS_MAX_MSG         256
#define BS_MAX_NODES       16
#define BS_AES_KEY_LEN     16
#define BS_CRC_POLY        0x1021  /* CRC-16-CCITT */

/* Message types */
enum bs_msg_type {
    BS_MSG_JOIN_REQ        = 0x01,
    BS_MSG_JOIN_ACK        = 0x02,
    BS_MSG_TELEMETRY       = 0x03,
    BS_MSG_COMMAND         = 0x04,
    BS_MSG_CMD_ACK         = 0x05,
    BS_MSG_ALERT           = 0x06,
    BS_MSG_OTA_BLOCK       = 0x07,
    BS_MSG_OTA_ACK         = 0x08,
    BS_MSG_HEARTBEAT       = 0x09,
    BS_MSG_TIME_SYNC       = 0x0B,
    BS_MSG_CONFIG          = 0x0C,
    BS_MSG_CONFIG_ACK      = 0x0D,
    BS_MSG_VITALS_STREAM   = 0x10,
    BS_MSG_IMU_STREAM      = 0x11,
    BS_MSG_NURSING_DATA    = 0x12,
    BS_MSG_WOUND_DATA      = 0x13,
    BS_MSG_VOICE_PROSODY   = 0x14,
    BS_MSG_RISK_UPDATE     = 0x15,
};

/* Telemetry sub-types */
enum bs_telem_subtype {
    BS_TELEM_RECOVERY_BAND  = 0x01,
    BS_TELEM_NURSING_SENSOR = 0x02,
    BS_TELEM_WOUND_PATCH    = 0x03,
    BS_TELEM_HUB            = 0x04,
};

/* Alert types */
enum bs_alert_type {
    BS_ALERT_HEMORRHAGE_RISK    = 0x01,
    BS_ALERT_HEMORRHAGE_HIGH    = 0x02,
    BS_ALERT_PREECLAMPSIA       = 0x03,
    BS_ALERT_WOUND_INFECTION    = 0x04,
    BS_ALERT_MASTITIS           = 0x05,
    BS_ALERT_PPD_SCREEN_POS     = 0x06,
    BS_ALERT_VITAL_ABNORMAL     = 0x07,
    BS_ALERT_SENSOR_OFFLINE     = 0x08,
    BS_ALERT_SENSOR_LOW_BATT    = 0x09,
    BS_ALERT_FEVER              = 0x0A,
    BS_ALERT_NURSING_REMINDER   = 0x0B,
    BS_ALERT_MEDICATION_REMINDER = 0x0C,
};

/* Alert severity */
enum bs_alert_severity {
    BS_SEV_INFO      = 0x00,
    BS_SEV_LOW       = 0x01,
    BS_SEV_MEDIUM    = 0x02,
    BS_SEV_HIGH      = 0x03,
    BS_SEV_CRITICAL  = 0x04,
};

/* Command sub-types */
enum bs_cmd_type {
    BS_CMD_START_MONITORING    = 0x01,
    BS_CMD_STOP_MONITORING     = 0x02,
    BS_CMD_CAPTURE_VOICE       = 0x03,
    BS_CMD_CALIBRATE           = 0x04,
    BS_CMD_REBOOT              = 0x05,
    BS_CMD_SET_CONFIG          = 0x06,
    BS_CMD_HAPTIC_ALERT        = 0x07,
    BS_CMD_AUDIO_MESSAGE       = 0x08,
    BS_CMD_EMERGENCY_ALERT     = 0x09,
    BS_CMD_SET_SAMPLE_RATE     = 0x0A,
};

/* Vitals data packet (10 bytes: HR, SpO2, skin_temp, HRV, activity_class) */
typedef struct __attribute__((packed)) {
    uint8_t  heart_rate;       /* bpm (0-255) */
    uint8_t  spo2;             /* % (0-100) */
    int16_t  skin_temp_cd;     /* skin temp centi-degrees (±0.01°C) */
    uint16_t hrv_rmssd_ms;     /* HRV RMSSD in ms */
    uint8_t  activity_class;   /* 0=rest,1=sit,2=walk,3=run,4=sleep,5=nurse */
    uint8_t  steps_count_lsb;  /* step count low byte (accumulated) */
    uint8_t  battery_pct;      /* 0-100 */
} bs_vitals_t;

/* IMU data packet (12 bytes: accel[3] + gyro[3], int16 each) */
typedef struct __attribute__((packed)) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
} bs_imu_sample_t;

/* Nursing data packet (10 bytes) */
typedef struct __attribute__((packed)) {
    int16_t  temp_left_cd;     /* left breast temp centi-degrees */
    int16_t  temp_right_cd;    /* right breast temp centi-degrees */
    int16_t  temp_asym_cd;     /* |left - right| centi-degrees */
    uint8_t  nursing_active;   /* 0=idle, 1=left, 2=right */
    uint8_t  position_id;      /* nursing position from IMU */
    uint8_t  battery_pct;
    uint8_t  reserved;
} bs_nursing_t;

/* Wound data packet (10 bytes) */
typedef struct __attribute__((packed)) {
    int16_t  wound_temp_cd;    /* wound temp centi-degrees */
    uint16_t moisture_raw;     /* capacitive moisture reading (0-65535) */
    uint8_t  moisture_pct;     /* 0-100% derived */
    uint8_t  ph_value;         /* pH × 10 (e.g., 74 = 7.4) */
    uint8_t  infection_risk;   /* 0-100 (from edge WoundInfect screening) */
    uint8_t  battery_pct;
    uint8_t  reserved;
} bs_wound_t;

/* Voice prosody features (128 bytes — 32 float32 features) */
typedef struct __attribute__((packed)) {
    float f0_mean;             /* mean fundamental frequency (Hz) */
    float f0_std;              /* F0 standard deviation */
    float f0_range;            /* F0 range (max-min) */
    float jitter_local;        /* local jitter (%) */
    float jitter_ppq5;         /* 5-point period perturbation quotient */
    float shimmer_local;       /* local shimmer (%) */
    float shimmer_apq11;       /* 11-point amplitude perturbation quotient */
    float hnr_db;              /* harmonics-to-noise ratio (dB) */
    float speech_rate;         /* syllables per second */
    float pause_ratio;         /* pause duration / total duration */
    float mean_intensity_db;   /* mean voice intensity (dB) */
    float intensity_var;       /* intensity variance */
    float spectral_slope;      /* spectral tilt/slope */
    float spectral_flux;       /* spectral change rate */
    float mfcc_1;              /* MFCC coefficient 1 (normalized) */
    float mfcc_2;              /* MFCC coefficient 2 */
    float mfcc_3;              /* MFCC coefficient 3 */
    float mfcc_4;              /* MFCC coefficient 4 */
    float breathiness;         /* breathiness ratio */
    float roughness;           /* roughness index */
    float pitch_declination;   /* F0 declination slope over utterance */
    float voiced_ratio;        /* voiced frames / total frames */
    float energy_mean;         /* mean RMS energy */
    float energy_std;          /* std RMS energy */
    float dur_phoneme_mean;    /* mean phoneme duration estimate */
    float dur_pause_mean;      /* mean pause duration */
    float dur_pause_std;       /* pause duration std */
    float f0_cv;               /* F0 coefficient of variation */
    float intensity_cv;        /* intensity coefficient of variation */
    float spectral_centroid;   /* spectral centroid (Hz) */
    float spectral_spread;     /* spectral spread (Hz) */
    float prosody_score;       /* composite prosody abnormality score 0-1 */
} bs_prosody_t;

/* Risk assessment (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t hemorrhage_risk;   /* 0-100 */
    uint8_t preeclampsia_risk; /* 0-100 */
    uint8_t wound_risk;        /* 0-100 */
    uint8_t mastitis_risk;     /* 0-100 */
    uint8_t ppd_risk;          /* 0-100 */
    uint8_t recovery_progress; /* 0-100 */
    uint8_t overall_risk;      /* 0-100 composite */
    uint8_t alert_level;       /* 0=normal,1=watch,2=warning,3=critical */
} bs_risk_t;

/* Message header (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  sync0;
    uint8_t  sync1;
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint8_t  subtype;
    uint8_t  seq;
    uint8_t  payload_len;
} bs_msg_header_t;

/* CRC-16-CCITT */
uint16_t bs_crc16(const uint8_t *data, size_t len);

/* Message encode (returns total message length including header + payload + crc) */
size_t bs_encode(uint8_t *out, size_t out_cap,
                 uint8_t src_id, uint8_t dst_id,
                 uint8_t msg_type, uint8_t subtype,
                 uint8_t seq, const uint8_t *payload, size_t payload_len);

/* Message decode (returns payload length, -1 on error) */
int bs_decode(const uint8_t *in, size_t in_len,
              bs_msg_header_t *hdr, uint8_t *payload, size_t payload_cap);

/* AES-128-CTR encrypt/decrypt (in-place) */
void bs_aes_encrypt(uint8_t *data, size_t len, const uint8_t *key, const uint8_t *nonce);

#endif /* BLOOMSYNC_PROTOCOL_H */
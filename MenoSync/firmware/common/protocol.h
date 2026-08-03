/*
 * MenoSync — Protocol Header
 * Binary message encoding/decoding for BLE WAN + Sub-GHz mesh.
 */
#ifndef MENOSYNC_PROTOCOL_H
#define MENOSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define MS_SYNC0           0x4D  /* 'M' */
#define MS_SYNC1           0x53  /* 'S' */
#define MS_BROADCAST       0xFF
#define MS_MAX_PAYLOAD     240
#define MS_MAX_MSG         256
#define MS_MAX_NODES       32
#define MS_AES_KEY_LEN     16
#define MS_CRC_POLY        0x1021  /* CRC-16-CCITT */

/* Message types */
enum ms_msg_type {
    MS_MSG_JOIN_REQ        = 0x01,
    MS_MSG_JOIN_ACK        = 0x02,
    MS_MSG_TELEMETRY       = 0x03,
    MS_MSG_COMMAND         = 0x04,
    MS_MSG_CMD_ACK         = 0x05,
    MS_MSG_ALERT           = 0x06,
    MS_MSG_OTA_BLOCK       = 0x07,
    MS_MSG_OTA_ACK         = 0x08,
    MS_MSG_HEARTBEAT       = 0x09,
    MS_MSG_TIME_SYNC       = 0x0B,
    MS_MSG_CONFIG          = 0x0C,
    MS_MSG_CONFIG_ACK      = 0x0D,
    MS_MSG_VITALS_STREAM   = 0x10,
    MS_MSG_EDA_STREAM      = 0x11,
    MS_MSG_IMU_STREAM      = 0x12,
    MS_MSG_BCG_STREAM      = 0x13,
    MS_MSG_SWEAT_DATA      = 0x14,
    MS_MSG_AMBIENT_DATA    = 0x15,
    MS_MSG_RADIANT_DATA    = 0x16,
    MS_MSG_VOICE_PROSODY   = 0x17,
    MS_MSG_HOTFLASH_PRED   = 0x18,
    MS_MSG_COOLING_CMD     = 0x19,
    MS_MSG_SLEEP_DATA      = 0x1A,
    MS_MSG_RISK_UPDATE     = 0x1B,
};

/* Telemetry sub-types */
enum ms_telem_subtype {
    MS_TELEM_WRIST_BAND   = 0x01,
    MS_TELEM_BED_MAT      = 0x02,
    MS_TELEM_CLIMATE      = 0x03,
    MS_TELEM_HUB          = 0x04,
};

/* Alert types */
enum ms_alert_type {
    MS_ALERT_HOTFLASH_PREDICTED  = 0x01,
    MS_ALERT_HOTFLASH_DETECTED   = 0x02,
    MS_ALERT_NIGHT_SWEAT         = 0x03,
    MS_ALERT_SLEEP_POOR          = 0x04,
    MS_ALERT_MOOD_CHANGE         = 0x05,
    MS_ALERT_BRAIN_FOG           = 0x06,
    MS_ALERT_BONE_RISK_HIGH      = 0x07,
    MS_ALERT_COOLING_ACTIVATED   = 0x08,
    MS_ALERT_COOLING_FAILED      = 0x09,
    MS_ALERT_SENSOR_OFFLINE      = 0x0A,
    MS_ALERT_SENSOR_LOW_BATT     = 0x0B,
    MS_ALERT_MEDICATION_REMINDER = 0x0C,
    MS_ALERT_VITAL_ABNORMAL      = 0x0D,
};

/* Alert severity */
enum ms_alert_severity {
    MS_SEV_INFO      = 0x00,
    MS_SEV_LOW       = 0x01,
    MS_SEV_MEDIUM    = 0x02,
    MS_SEV_HIGH      = 0x03,
    MS_SEV_CRITICAL  = 0x04,
};

/* Command sub-types */
enum ms_cmd_type {
    MS_CMD_START_MONITORING    = 0x01,
    MS_CMD_STOP_MONITORING     = 0x02,
    MS_CMD_CAPTURE_VOICE       = 0x03,
    MS_CMD_CALIBRATE           = 0x04,
    MS_CMD_REBOOT              = 0x05,
    MS_CMD_SET_CONFIG          = 0x06,
    MS_CMD_HAPTIC_ALERT        = 0x07,
    MS_CMD_AUDIO_MESSAGE       = 0x08,
    MS_CMD_COOLING_START       = 0x09,
    MS_CMD_COOLING_STOP        = 0x0A,
    MS_CMD_SET_SAMPLE_RATE     = 0x0B,
    MS_CMD_SET_HVAC_TEMP       = 0x0C,
    MS_CMD_SET_SHADE_PCT       = 0x0D,
};

/* Vitals data packet (10 bytes: HR, SpO2, skin_temp, HRV, activity_class) */
typedef struct __attribute__((packed)) {
    uint8_t  heart_rate;       /* bpm (0-255) */
    uint8_t  spo2;             /* % (0-100) */
    int16_t  skin_temp_cd;     /* skin temp centi-degrees (±0.01°C) */
    uint16_t hrv_rmssd_ms;     /* HRV RMSSD in ms */
    uint8_t  activity_class;   /* 0=rest,1=sit,2=walk,3=run,4=sleep,5=stretch */
    uint8_t  steps_count_lsb;  /* step count low byte (accumulated) */
    uint8_t  battery_pct;      /* 0-100 */
} ms_vitals_t;

/* EDA data packet (8 bytes: skin conductance, skin conductance std) */
typedef struct __attribute__((packed)) {
    uint16_t eda_microsiemens; /* skin conductance (µS) */
    uint16_t eda_std;          /* std deviation over 15s window (µS) */
    uint8_t  eda_tonic;        /* tonic level (baseline) */
    uint8_t  eda_phasic;       /* phasic component (event-related) */
    uint8_t  stress_level;     /* 0=calm,1=low,2=moderate,3=high */
    uint8_t  reserved;
} ms_eda_t;

/* IMU data packet (12 bytes: accel[3] + gyro[3], int16 each) */
typedef struct __attribute__((packed)) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
} ms_imu_sample_t;

/* BCG data packet (8 bytes: heart rate, breathing rate, motion) */
typedef struct __attribute__((packed)) {
    uint8_t  hr_bpm;           /* heart rate from BCG (bpm) */
    uint8_t  br_bpm;           /* breathing rate (breaths per min) */
    uint8_t  motion_level;     /* 0=still, 1=light, 2=moving, 3=restless */
    uint8_t  sleep_stage;      /* 0=awake, 1=light, 2=deep, 3=REM */
    uint8_t  battery_pct;
    uint8_t  signal_quality;   /* 0-100 (PIEZO signal quality) */
    uint16_t reserved;
} ms_bcg_t;

/* Sweat data packet (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t sweat_raw;        /* FDC2214 raw capacitance */
    uint8_t  sweat_pct;        /* 0-100% moisture above baseline */
    uint8_t  night_sweat_flag; /* 0=none, 1=mild, 2=severe (edge screening) */
    int16_t  bed_temp_cd;      /* mattress surface temp (centi-°C) */
    uint8_t  battery_pct;
    uint8_t  reserved;
} ms_sweat_t;

/* Ambient data packet (10 bytes) */
typedef struct __attribute__((packed)) {
    int16_t  ambient_temp_cd;  /* ambient temp (centi-°C) */
    uint16_t humidity_pct;     /* relative humidity % */
    uint16_t pressure_hpa;     /* atmospheric pressure (hPa) */
    int16_t  radiant_temp_cd;  /* MLX90640 average radiant temp (centi-°C) */
    uint8_t  hvac_state;       /* 0=off, 1=cooling, 2=heating, 3=fan */
    uint8_t  shade_pct;        /* 0=open, 100=fully closed */
} ms_ambient_t;

/* Cooling command (6 bytes — Hub → Climate Node via Sub-GHz) */
typedef struct __attribute__((packed)) {
    uint8_t  action;           /* 0=stop, 1=start cooling, 2=set HVAC temp, 3=set shade */
    int16_t  target_temp_cd;   /* target ambient temp (centi-°C) */
    uint8_t  hvac_mode;        /* 0=off, 1=cool, 2=heat, 3=fan, 4=auto */
    uint8_t  shade_pct;        /* 0-100 (shade closure percentage) */
} ms_cooling_cmd_t;

/* Hot flash prediction (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  probability;      /* 0-100 (probability of hot flash in next 15 min) */
    uint8_t  minutes_to_onset; /* estimated minutes to onset (0-20) */
    uint8_t  skin_temp_trend;  /* 0=stable, 1=rising, 2=rapidly rising */
    uint8_t  eda_trend;        /* 0=stable, 1=rising, 2=spike */
    uint8_t  trigger_flags;    /* bit0=ambient temp, bit1=stress, bit2=activity */
    uint8_t  severity_pred;    /* 0=mild, 1=moderate, 2=severe (predicted) */
    uint8_t  cooling_recommended; /* 0=no, 1=yes */
    uint8_t  confidence;       /* 0-100 model confidence */
} ms_hotflash_pred_t;

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
} ms_prosody_t;

/* Risk assessment (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t hotflash_risk;     /* 0-100 */
    uint8_t nightsweat_risk;   /* 0-100 */
    uint8_t sleep_quality;     /* 0-100 (100 = best sleep) */
    uint8_t mood_risk;         /* 0-100 */
    uint8_t bone_risk;         /* 0-100 */
    uint8_t overall_risk;      /* 0-100 composite */
    uint8_t cooling_active;    /* 0=no, 1=yes */
    uint8_t alert_level;       /* 0=normal,1=watch,2=warning,3=critical */
} ms_risk_t;

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
} ms_msg_header_t;

/* CRC-16-CCITT */
uint16_t ms_crc16(const uint8_t *data, size_t len);

/* Message encode (returns total message length including header + payload + crc) */
size_t ms_encode(uint8_t *out, size_t out_cap,
                 uint8_t src_id, uint8_t dst_id,
                 uint8_t msg_type, uint8_t subtype,
                 uint8_t seq, const uint8_t *payload, size_t payload_len);

/* Message decode (returns payload length, -1 on error) */
int ms_decode(const uint8_t *in, size_t in_len,
              ms_msg_header_t *hdr, uint8_t *payload, size_t payload_cap);

/* AES-128-CTR encrypt/decrypt (in-place) */
void ms_aes_encrypt(uint8_t *data, size_t len, const uint8_t *key, const uint8_t *nonce);

#endif /* MENOSYNC_PROTOCOL_H */
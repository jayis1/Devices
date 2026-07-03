#ifndef DRIVESYNC_CAMERA_DRIVER_H
#define DRIVESYNC_CAMERA_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * OV5640 camera driver for DriveSync Dash Hub.
 * Driver-facing IR camera with 940 nm illumination.
 * Captures 640x480 grayscale at 10 FPS for eye-closure/head-pose inference.
 */

/* Camera configuration */
#define CAM_WIDTH       640
#define CAM_HEIGHT      480
#define CAM_FPS         10
#define CAM_FORMAT      PIXFORMAT_GRAYSCALE

/* IR illumination */
#define IR_LED_GPIO     21

/* Alert sounds (I2S → MAX98357A) */
typedef enum {
    ALERT_LOW = 0,
    ALERT_MODERATE,
    ALERT_HIGH,
    ALERT_URGENT,
} alert_sound_t;

/* Voice prompts */
typedef enum {
    VOICE_PULL_OVER = 0,
    VOICE_TAKE_BREAK,
    VOICE_STAY_ALERT,
} voice_prompt_t;

/* Camera features (extracted by edge inference) */
typedef struct {
    float    perclos;         /* 0.0-1.0 */
    uint16_t blink_rate;      /* blinks/min */
    uint16_t avg_blink_dur;   /* ms */
    int16_t  head_pitch;      /* centi-degrees */
    int16_t  head_yaw;
    int16_t  head_roll;
    uint8_t  head_bob_count;
    uint8_t  confidence;      /* 0-100 */
} camera_features_t;

/* Callback for processed frames */
typedef void (*camera_frame_cb_t)(const camera_features_t *features);

/**
 * Initialize camera + IR LED.
 */
void camera_init(void);

/**
 * Start continuous capture at given FPS.
 * Frames are processed by edge inference and results passed to callback.
 */
void camera_start_capture(uint8_t fps);

/**
 * Stop capture.
 */
void camera_stop_capture(void);

/**
 * Set callback for processed frame features.
 */
void camera_set_frame_callback(camera_frame_cb_t cb);

/**
 * Control IR LED illumination (auto-controlled, but can be overridden).
 */
void camera_set_ir_enabled(bool enabled);

/**
 * Play alert sound through I2S speaker.
 */
void camera_play_alert_sound(alert_sound_t sound);

/**
 * Play voice prompt through I2S speaker.
 */
void camera_play_voice_prompt(voice_prompt_t prompt);

#endif /* DRIVESYNC_CAMERA_DRIVER_H */
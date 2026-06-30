/**
 * AsthmaSync — Inhaler Tag Actuation Header
 *
 * License: MIT
 */

#ifndef ACTUATION_H
#define ACTUATION_H

#include "../common/protocol.h"
#include <stdint.h>

/* Actuation classes */
enum {
    ACT_CLASS_STATIC       = 0,
    ACT_CLASS_ACTUATION    = 1,
    ACT_CLASS_POCKET_SHAKE = 2,
    ACT_CLASS_DROP         = 3,
};

/* Accelerometer sample */
typedef struct {
    float x, y, z;       /* acceleration in g */
    float magnitude;     /* sqrt(x² + y² + z²) */
    float jerk;          /* |magnitude - prev_magnitude| */
} accel_sample_t;

/* Callback type for actuation events */
typedef void (*actuation_callback_t)(const actuation_t *actuation);

/** Initialize LSM6DSO IMU. */
int lsm6dso_init(void);

/** Read current acceleration (3-axis, in g). */
int lsm6dso_read_accel(float *x_g, float *y_g, float *z_g);

/** Called when LSM6DSO INT1 fires (wake-up threshold exceeded). */
void actuation_on_interrupt(void);

/** Register callback for confirmed actuation events. */
void actuation_set_callback(actuation_callback_t cb);

/** Get total dose count since reset. */
uint16_t actuation_get_dose_count(void);

/** Get last actuation data. */
const actuation_t* actuation_get_last(void);

/** Reset dose count (when new inhaler is attached). */
void actuation_reset_dose_count(void);

#endif /* ACTUATION_H */
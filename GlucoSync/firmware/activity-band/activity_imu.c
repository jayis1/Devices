/**
 * LSM6DSO IMU driver + activity classification (TinyCNN).
 * 5-class: sedentary, walk, run, bike, strength.
 * License: MIT
 */

#include "activity_imu.h"
#include <string.h>
#include <math.h>

static bool g_initialized = false;
static uint8_t g_sample_rate = 50;

void activity_imu_init(void)
{
    /* Production: I²C init on P0.27(SDA)/P0.28(SCL) @ 400kHz
     * LSM6DSO register init:
     * - CTRL1_XL: 0x40 (104Hz accel, ±2g)
     * - CTRL2_G: 0x40 (104Hz gyro, ±250dps)
     * - CTRL3_C: 0x04 (BDU enable)
     */
    g_initialized = true;
}

void activity_imu_set_sample_rate(uint8_t hz)
{
    g_sample_rate = hz;
}

void activity_imu_read_and_classify(activity_result_t *result)
{
    if (!g_initialized || result == NULL) return;

    memset(result, 0, sizeof(*result));

    /* Production pipeline:
     * 1. Read accel + gyro FIFO (50 samples at 50 Hz = 1 second window)
     * 2. Extract features:
     *    - Mean abs accel magnitude
     *    - Std of accel magnitude (signal variability)
     *    - Frequency domain: FFT peak frequency (steps/sec)
     *    - Gyro variance (rotation = walking/running)
     *    - Step count (peak detection on accel)
     * 3. Run TinyCNN classifier (1D conv → dense → softmax)
     * 4. Output class + confidence
     */

    /* Placeholder: assume sedentary */
    result->class_id = 0;
    result->intensity = 0;
    result->confidence = 0;
}
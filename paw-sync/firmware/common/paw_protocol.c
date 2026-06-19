/*
 * paw_protocol.c — PawSync shared mesh protocol implementation
 *
 * CRC16-CCITT computation, payload CRC packing/verification, and helper
 * functions for gait symmetry and activity classification.
 *
 * SPDX-License-Identifier: MIT
 */
#include "paw_protocol.h"

/* ---- CRC16-CCITT (poly 0x1021, init 0xFFFF, no reflection) ---- */
uint16_t paw_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * Pack CRC into the last 2 bytes of a payload struct.
 * `struct_size_without_crc` = sizeof(struct) - 2
 * Returns the computed CRC value.
 */
uint16_t paw_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint16_t crc = paw_crc16((const uint8_t *)payload, struct_size_without_crc);
    /* Write CRC into the trailing 2 bytes (little-endian) */
    uint8_t *p = (uint8_t *)payload + struct_size_without_crc;
    p[0] = (uint8_t)(crc & 0xFF);
    p[1] = (uint8_t)((crc >> 8) & 0xFF);
    return crc;
}

/*
 * Verify the CRC of a received payload.
 * Returns 0 if valid, -1 if mismatch.
 */
int paw_verify_crc(const void *payload, size_t struct_size_without_crc, uint16_t received_crc)
{
    uint16_t computed = paw_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (computed == received_crc) ? 0 : -1;
}

/* ---- Gait symmetry helpers ---- */

/*
 * Compute gait symmetry index from bilateral stride data.
 * 0 = perfectly symmetric, higher = more asymmetric.
 * Range: 0.0 - 1.0 (scaled to 0-1000 in protocol)
 *
 * Uses the standard symmetry index:
 *   SI = |L - R| / max(L, R)
 *
 * @param stride_l  Left stride length (cm)
 * @param stride_r  Right stride length (cm)
 * @return symmetry index 0-1000 (0=perfect)
 */
uint16_t paw_gait_symmetry_index(int16_t stride_l, int16_t stride_r)
{
    if (stride_l <= 0 || stride_r <= 0)
        return 0;
    int16_t diff = stride_l - stride_r;
    if (diff < 0) diff = -diff;
    int16_t mx = (stride_l > stride_r) ? stride_l : stride_r;
    /* SI * 1000 */
    return (uint16_t)((diff * 1000) / mx);
}

/*
 * Classify lameness severity from gait symmetry index.
 * 0 = normal, 1-4 = increasing severity.
 *
 * Thresholds from veterinary gait analysis literature:
 *   SI < 0.04 (40)   → normal
 *   SI 0.04-0.08 (40-80) → mild (grade 1)
 *   SI 0.08-0.15 (80-150) → moderate (grade 2-3)
 *   SI > 0.15 (150) → severe (grade 4)
 */
uint8_t paw_lameness_grade(uint16_t symmetry_idx_1000)
{
    if (symmetry_idx_1000 < 40)  return 0;  /* normal */
    if (symmetry_idx_1000 < 80)  return 1;  /* mild */
    if (symmetry_idx_1000 < 150) return 2;  /* moderate */
    if (symmetry_idx_1000 < 250) return 3;  /* marked */
    return 4;                                 /* severe */
}

/*
 * Detect scratching episode from IMU high-frequency vibration.
 * Returns 1 if a scratching pattern is detected in the window, 0 otherwise.
 *
 * Scratching signature: sustained high-frequency (8-12Hz) acceleration
 * on the vertical axis with low overall body displacement.
 *
 * @param accel_z   Vertical acceleration samples (50Hz, window of 100 = 2s)
 * @param n         Number of samples
 * @param threshold Peak-to-peak threshold for scratching detection
 */
int paw_detect_scratching(const int16_t *accel_z, int n, int16_t threshold)
{
    if (n < 50) return 0;
    int scratch_peaks = 0;
    for (int i = 1; i < n - 1; i++) {
        if (accel_z[i] > threshold && accel_z[i] > accel_z[i-1] && accel_z[i] > accel_z[i+1])
            scratch_peaks++;
        if (accel_z[i] < -threshold && accel_z[i] < accel_z[i-1] && accel_z[i] < accel_z[i+1])
            scratch_peaks++;
    }
    /* Expect 8-12Hz scratching → ~16-24 peaks in 2s window */
    return (scratch_peaks >= 12) ? 1 : 0;
}

/*
 * Detect head-shaking from gyroscope rotational data.
 * Returns 1 if head-shaking detected, 0 otherwise.
 *
 * Head-shaking signature: rapid rotational oscillation on the yaw axis
 * (typically 3-6Hz, high amplitude).
 *
 * @param gyro_z   Yaw rotational velocity samples (50Hz, 2s window)
 * @param n         Number of samples
 * @param threshold Rotational velocity threshold (mdps)
 */
int paw_detect_head_shake(const int16_t *gyro_z, int n, int16_t threshold)
{
    if (n < 50) return 0;
    int direction_changes = 0;
    int16_t prev = gyro_z[0];
    for (int i = 1; i < n; i++) {
        if (prev > threshold && gyro_z[i] < -threshold)
            direction_changes++;
        else if (prev < -threshold && gyro_z[i] > threshold)
            direction_changes++;
        prev = gyro_z[i];
    }
    /* Head shake: 3-6Hz → expect 6-12 direction changes in 2s */
    return (direction_changes >= 5) ? 1 : 0;
}
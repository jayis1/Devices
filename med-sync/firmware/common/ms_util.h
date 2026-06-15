/**
 * MedSync - Utility Functions
 * Common helpers used by all nodes
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#ifndef MS_UTIL_H
#define MS_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================
 * Bit Manipulation Helpers
 * ============================================================ */

static inline void ms_set_bit(uint8_t *byte, uint8_t bit) {
    *byte |= (1U << bit);
}

static inline void ms_clear_bit(uint8_t *byte, uint8_t bit) {
    *byte &= ~(1U << bit);
}

static inline bool ms_get_bit(uint8_t byte, uint8_t bit) {
    return (byte >> bit) & 1U;
}

/* ============================================================
 * Time Helpers
 * ============================================================ */

/* Convert hour/minute to minutes-since-midnight */
static inline uint16_t ms_time_to_minutes(uint8_t hour, uint8_t minute) {
    return (uint16_t)hour * 60 + minute;
}

/* Convert minutes-since-midnight to hour/minute */
static inline void ms_minutes_to_time(uint16_t minutes, uint8_t *hour, uint8_t *minute) {
    *hour = (uint8_t)(minutes / 60);
    *minute = (uint8_t)(minutes % 60);
}

/* Check if current time matches a schedule entry */
static inline bool ms_is_schedule_active(uint8_t sched_hour, uint8_t sched_min,
                                          uint8_t current_hour, uint8_t current_min,
                                          uint8_t tolerance_min) {
    int16_t sched_total = (int16_t)sched_hour * 60 + sched_min;
    int16_t curr_total = (int16_t)current_hour * 60 + current_min;
    int16_t diff = curr_total - sched_total;

    if (diff < 0) diff += 24 * 60; /* Wrap around midnight */

    return (diff >= 0 && diff <= tolerance_min);
}

/* ============================================================
 * Weight/Scale Helpers
 * ============================================================ */

/* Calculate expected weight change for a dose */
static inline int32_t ms_dose_weight_mg(uint8_t pill_count, uint16_t pill_weight_mg) {
    return (int32_t)pill_count * (int32_t)pill_weight_mg;
}

/* Check if weight change is within tolerance of expected dose */
static inline bool ms_weight_confirms_dose(int32_t measured_change_mg,
                                             int32_t expected_change_mg,
                                             uint16_t tolerance_mg) {
    int32_t diff = measured_change_mg - expected_change_mg;
    if (diff < 0) diff = -diff;
    return diff <= tolerance_mg;
}

/* ============================================================
 * Battery Helpers
 * ============================================================ */

/* Convert ADC reading to battery voltage (mV) with voltage divider */
static inline uint16_t ms_adc_to_battery_mv(uint16_t adc_value, uint16_t adc_max,
                                               uint32_t r1_ohm, uint32_t r2_ohm) {
    /* Vbat = Vadc × (R1 + R2) / R2 */
    uint32_t vadc_mv = (uint32_t)adc_value * 3300 / adc_max;
    return (uint16_t)(vadc_mv * (r1_ohm + r2_ohm) / r2_ohm);
}

/* Check battery level */
static inline bool ms_battery_low(uint16_t battery_mv) {
    return battery_mv < MS_BATTERY_LOW_MV;
}

static inline bool ms_battery_critical(uint16_t battery_mv) {
    return battery_mv < MS_BATTERY_CRITICAL_MV;
}

/* ============================================================
 * String Helpers
 * ============================================================ */

/* Copy string with guaranteed null termination */
static inline void ms_str_copy(char *dst, const char *src, size_t dst_size) {
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

#endif /* MS_UTIL_H */
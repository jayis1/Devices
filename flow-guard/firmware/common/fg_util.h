/**
 * FlowGuard - Utility functions shared across all nodes
 * Sensor helpers, battery measurement, leak detection helpers
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#ifndef FG_UTIL_H
#define FG_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include "fg_protocol.h"

/* ============================================================
 * Battery voltage measurement
 * ============================================================ */

/**
 * Convert ADC raw value to battery voltage in mV.
 * Uses voltage divider: Vbat ---[R1]---+---[R2]--- GND
 *                                     |
 *                                  ADC input
 *
 * @param adc_raw  Raw ADC reading (12-bit, 0-4095)
 * @param vref_mv  Reference voltage in mV (typically 3300)
 * @param r1_ohm   Upper resistor in divider (typ. 1MΩ)
 * @param r2_ohm   Lower resistor in divider (typ. 1MΩ)
 * @return Battery voltage in mV
 */
static inline uint16_t fg_adc_to_battery_mv(uint16_t adc_raw, uint16_t vref_mv,
                                              uint32_t r1_ohm, uint32_t r2_ohm)
{
    /* Vdiv = adc_raw * vref / 4096 */
    /* Vbat = Vdiv * (R1 + R2) / R2 */
    uint32_t vdiv_mv = ((uint32_t)adc_raw * vref_mv) / 4096;
    uint32_t vbat_mv = (vdiv_mv * (r1_ohm + r2_ohm)) / r2_ohm;
    return (uint16_t)vbat_mv;
}

/**
 * Classify battery level based on voltage.
 * Works for both AA batteries (3V nominal) and CR2477 (3V nominal).
 *
 * @param battery_mv Battery voltage in mV
 * @return Battery level: 0=critical, 1=low, 2=ok, 3=full
 */
static inline uint8_t fg_battery_level(uint16_t battery_mv)
{
    if (battery_mv < FG_BATTERY_CRITICAL_MV) return 0;  /* Critical */
    if (battery_mv < FG_BATTERY_LOW_MV)      return 1;  /* Low */
    if (battery_mv < 2900)                    return 2;  /* OK */
    return 3;  /* Full */
}

/* ============================================================
 * Temperature conversion
 * ============================================================ */

/**
 * Convert DS18B20 raw temperature register to °C × 100 format.
 * DS18B20 outputs 16-bit: S10S9...S1S0 2^-1 2^-2 2^-3 2^-4
 * Resolution: 0.0625°C in 12-bit mode
 *
 * @param raw  DS18B20 raw temperature register value
 * @return Temperature in °C × 100 (e.g., 2345 = 23.45°C)
 */
static inline int16_t fg_ds18b20_to_cx100(int16_t raw)
{
    /* raw is in 0.0625°C units, multiply by 6.25 to get 0.01°C units */
    /* 6.25 = 25/4, so: (raw * 25) / 4 */
    return (int16_t)(((int32_t)raw * 25) / 4);
}

/* ============================================================
 * Flow rate calculations
 * ============================================================ */

/**
 * Convert YF-S201 pulse frequency to flow rate in mL/min.
 * YF-S201: 1 pulse per 2.25 mL, so frequency (Hz) × 2.25 × 60 = mL/min
 * But more accurately: flow_rate (L/min) = pulse_freq (Hz) / 7.5
 * mL/min = L/min × 1000
 *
 * @param pulse_freq_hz  Pulse frequency in Hz
 * @return Flow rate in mL/min
 */
static inline uint16_t fg_pulse_to_flow_ml_min(float pulse_freq_hz)
{
    /* L/min = Hz / 7.5 */
    /* mL/min = L/min × 1000 = Hz × 1000 / 7.5 = Hz × 133.33 */
    return (uint16_t)(pulse_freq_hz * 133.33f);
}

/**
 * Convert pulse count to volume in mL.
 * YF-S201: 1 pulse = 2.25 mL
 *
 * @param pulse_count  Number of pulses counted
 * @return Volume in mL
 */
static inline uint32_t fg_pulse_to_volume_ml(uint32_t pulse_count)
{
    return pulse_count * 2.25f;
}

/* ============================================================
 * Pressure conversion
 * ============================================================ */

/**
 * Convert MPX5700DP analog reading to pressure in kPa × 10.
 * MPX5700DP: 0.2V at 0 kPa, 4.7V at 700 kPa
 * Transfer: Vout = 0.00644 × P + 0.2  (P in kPa)
 * Therefore: P = (Vout - 0.2) / 0.00644
 *
 * @param adc_raw    Raw ADC reading (12-bit, 0-4095)
 * @param vref_mv    ADC reference voltage (typically 3300mV)
 * @return Pressure in kPa × 10 (e.g., 3450 = 345.0 kPa ≈ 50 PSI)
 */
static inline int16_t fg_mpx5700_to_kpa_x10(uint16_t adc_raw, uint16_t vref_mv)
{
    /* Vout_mv = adc_raw * vref_mv / 4096 */
    uint32_t vout_mv = ((uint32_t)adc_raw * vref_mv) / 4096;
    /* P_kpa = (Vout - 200mV) / 6.44 mV/kPa */
    /* P_kpa_x10 = (Vout - 200) * 10 / 6.44 */
    int32_t p_kpa_x10 = ((int32_t)vout_mv - 200) * 10;
    /* Avoid division by using integer approximation: / 6.44 ≈ * 155 / 1000 */
    p_kpa_x10 = (p_kpa_x10 * 155) / 1000;
    return (p_kpa_x10 < 0) ? 0 : (int16_t)p_kpa_x10;
}

/* ============================================================
 * Leak detection helpers
 * ============================================================ */

/**
 * Debounced leak detection.
 * Requires N consecutive positive readings before confirming leak.
 *
 * @param current_reading  Current conductivity probe reading (true=wet)
 * @param counter          Pointer to debounce counter (initialized to 0)
 * @param threshold        Number of consecutive readings required
 * @return true if leak is confirmed after debounce
 */
static inline bool fg_leak_debounce(bool current_reading, uint8_t *counter, uint8_t threshold)
{
    if (current_reading) {
        (*counter)++;
        if (*counter >= threshold) {
            *counter = threshold;  /* Saturate */
            return true;
        }
    } else {
        *counter = 0;
    }
    return false;
}

/* ============================================================
 * String helpers
 * ============================================================ */

/**
 * Convert node type enum to human-readable string.
 */
static inline const char *fg_node_type_str(fg_node_type_t type)
{
    switch (type) {
        case FG_NODE_HUB:               return "Hub";
        case FG_NODE_VALVE_CTRL:        return "ValveCtrl";
        case FG_NODE_PIPE_SENSOR:       return "PipeSensor";
        case FG_NODE_APPLIANCE_MONITOR: return "ApplianceMon";
        default:                         return "Unknown";
    }
}

/**
 * Convert valve state to string.
 */
static inline const char *fg_valve_state_str(fg_valve_state_t state)
{
    switch (state) {
        case FG_VALVE_OPEN:     return "OPEN";
        case FG_VALVE_CLOSED:   return "CLOSED";
        case FG_VALVE_CLOSING:  return "CLOSING";
        case FG_VALVE_OPENING:  return "OPENING";
        case FG_VALVE_ERROR:    return "ERROR";
        default:                return "UNKNOWN";
    }
}

/**
 * Convert alert level to string.
 */
static inline const char *fg_alert_level_str(fg_alert_level_t level)
{
    switch (level) {
        case FG_ALERT_INFO:      return "INFO";
        case FG_ALERT_WARNING:   return "WARNING";
        case FG_ALERT_CRITICAL:  return "CRITICAL";
        case FG_ALERT_EMERGENCY: return "EMERGENCY";
        default:                 return "UNKNOWN";
    }
}

#endif /* FG_UTIL_H */
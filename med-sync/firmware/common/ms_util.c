/**
 * MedSync - Utility Functions Implementation
 * ms_util.c - Common utility functions for all nodes
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include "ms_util.h"

/* ---- Time Helpers ---- */

void ms_time_to_rtc(uint32_t unix_ts, ms_rtc_time_t *out)
{
    /* Simplified Unix-to-calendar conversion (2024–2099) */
    uint32_t days = unix_ts / 86400;
    uint32_t rem  = unix_ts % 86400;

    out->hour   = rem / 3600;
    out->minute = (rem % 3600) / 60;
    out->second = rem % 60;

    /* Day of week: 2024-01-01 was Monday */
    out->weekday = (days + 0) % 7; /* 0=Mon */

    /* Year and day-of-year */
    uint32_t year = 2024;
    for (;;) {
        uint32_t diy = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
                        ? 366 : 365;
        if (days < diy) break;
        days -= diy;
        year++;
    }
    out->year = year;

    /* Month and day */
    static const uint8_t mdays[2][12] = {
        {31,28,31,30,31,30,31,31,30,31,30,31},
        {31,29,31,30,31,30,31,31,30,31,30,31},
    };
    uint8_t leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint8_t m;
    for (m = 0; m < 12; m++) {
        if (days < mdays[leap][m]) break;
        days -= mdays[leap][m];
    }
    out->month = m + 1;
    out->day   = days + 1;
}

uint32_t ms_rtc_to_unix(const ms_rtc_time_t *t)
{
    /* Convert calendar to Unix timestamp (simplified) */
    uint32_t days = 0;
    for (uint32_t y = 2024; y < t->year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }

    static const uint8_t mdays[2][12] = {
        {31,28,31,30,31,30,31,31,30,31,30,31},
        {31,29,31,30,31,30,31,31,30,31,30,31},
    };
    uint8_t leap = (t->year % 4 == 0 && (t->year % 100 != 0 || t->year % 400 == 0));
    for (uint8_t m = 0; m < t->month - 1; m++) {
        days += mdays[leap][m];
    }
    days += t->day - 1;

    return days * 86400 + t->hour * 3600 + t->minute * 60 + t->second;
}

bool ms_is_dose_time(const ms_rtc_time_t *now, uint8_t sched_hour,
                      uint8_t sched_min, uint8_t tolerance_min)
{
    int32_t diff = (int32_t)(now->hour * 60 + now->minute)
                 - (int32_t)(sched_hour * 60 + sched_min);
    if (diff < 0) diff = -diff;
    return diff <= tolerance_min;
}

/* ---- Battery Helpers ---- */

uint8_t mv_to_battery_pct(uint16_t mv)
{
    /* Approximate Li-Ion / LiFePO4 discharge curve */
    if (mv >= 4200) return 100;
    if (mv >= 4000) return (uint8_t)(80 + (mv - 4000) * 20 / 200);
    if (mv >= 3700) return (uint8_t)(20 + (mv - 3700) * 60 / 300);
    if (mv >= 3400) return (uint8_t)((mv - 3400) * 20 / 300);
    return 0;
}

/* ---- Sensor Helpers ---- */

float ms_q15_to_float(int16_t q15)
{
    return (float)q15 / 32768.0f;
}

int16_t ms_float_to_q15(float f)
{
    if (f >= 1.0f) return 32767;
    if (f <= -1.0f) return -32768;
    return (int16_t)(f * 32768.0f);
}

uint16_t ms_celsius_to_cx100(float c)
{
    return (uint16_t)(c * 100.0f + 0.5f);
}

float ms_cx100_to_celsius(uint16_t cx100)
{
    return (float)cx100 / 100.0f;
}

/* ---- Activity Classification ---- */

ms_activity_t ms_classify_activity(int16_t accel_mag_q15, int16_t gyro_mag_q15)
{
    float am = ms_q15_to_float(accel_mag_q15);
    float gm = ms_q15_to_float(gyro_mag_q15);

    if (am < 0.02f && gm < 0.05f) {
        return MS_ACTIVITY_STILL;
    }
    if (am > 0.5f && gm > 0.3f) {
        return MS_ACTIVITY_RUNNING;
    }
    if (am > 0.03f) {
        return MS_ACTIVITY_WALKING;
    }
    return MS_ACTIVITY_UNKNOWN;
}

/* ---- String Helpers ---- */

const char *ms_dose_status_str(uint8_t status)
{
    switch (status) {
    case MS_DOSE_PENDING:      return "PENDING";
    case MS_DOSE_DISPENSED:    return "DISPENSED";
    case MS_DOSE_PROBABLY:     return "PROBABLY_TAKEN";
    case MS_DOSE_CONFIRMED:    return "CONFIRMED";
    case MS_DOSE_OVERDUE:      return "OVERDUE";
    case MS_DOSE_MISSED:       return "MISSED";
    case MS_DOSE_SKIPPED:      return "SKIPPED";
    default:                   return "UNKNOWN";
    }
}

const char *ms_alert_level_str(uint8_t level)
{
    switch (level) {
    case MS_ALERT_INFO:     return "INFO";
    case MS_ALERT_REMINDER: return "REMINDER";
    case MS_ALERT_WARNING:  return "WARNING";
    case MS_ALERT_URGENT:   return "URGENT";
    case MS_ALERT_EMERGENCY:return "EMERGENCY";
    default:                return "UNKNOWN";
    }
}

const char *ms_activity_str(uint8_t activity)
{
    switch (activity) {
    case MS_ACTIVITY_STILL:   return "STILL";
    case MS_ACTIVITY_WALKING:  return "WALKING";
    case MS_ACTIVITY_RUNNING:  return "RUNNING";
    case MS_ACTIVITY_SLEEPING: return "SLEEPING";
    case MS_ACTIVITY_UNKNOWN:  return "UNKNOWN";
    default:                   return "UNKNOWN";
    }
}

/* ---- Queue Helpers ---- */

void ms_pkt_queue_init(ms_pkt_queue_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool ms_pkt_queue_push(ms_pkt_queue_t *q, const ms_mesh_packet_t *pkt)
{
    if (q->count >= MS_PKT_QUEUE_SIZE) {
        return false; /* Full */
    }
    q->buf[q->tail] = *pkt;
    q->tail = (q->tail + 1) % MS_PKT_QUEUE_SIZE;
    q->count++;
    return true;
}

bool ms_pkt_queue_pop(ms_pkt_queue_t *q, ms_mesh_packet_t *out)
{
    if (q->count == 0) {
        return false; /* Empty */
    }
    *out = q->buf[q->head];
    q->head = (q->head + 1) % MS_PKT_QUEUE_SIZE;
    q->count--;
    return true;
}

/* ---- Weight Helpers ---- */

int32_t ms_raw_to_mg(int32_t raw, int32_t offset, int32_t scale)
{
    /* Convert raw HX711 reading to milligrams */
    if (scale == 0) return 0;
    return ((raw - offset) * 1000) / scale;
}

int32_t mg_to_raw(int32_t mg, int32_t offset, int32_t scale)
{
    /* Convert milligrams back to raw HX711 reading */
    return (mg * scale) / 1000 + offset;
}
/*
 * circadian.c — CalmGrid circadian lighting schedule + de-stress scenes
 *
 * Manages the circadian lighting schedule and de-stress scene transitions.
 * Circadian rhythm is the body's internal 24-hour clock; proper light
 * exposure (cool bright by day, warm dim by evening) entrains it for
 * better sleep, mood, and stress resilience.
 *
 * Circadian schedule:
 *   06:00-09:00: Sunrise (warm 2700K → cool 5000K, 10% → 80% brightness)
 *   09:00-12:00: Morning work (cool 5500K, 90%)
 *   12:00-17:00: Afternoon work (cool 5000K, 80%)
 *   17:00-20:00: Sunset (cool → warm, 80% → 40%)
 *   20:00-22:00: Evening (warm 2700K, 30% → 10%)
 *   22:00-06:00: Night (off or very dim warm 2200K, 0-5%)
 *
 * De-stress scene: warm 2700K at 30-40% brightness — proven to reduce
 * sympathetic arousal compared to cool bright light.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "calm_protocol.h"

/* ---- Scene target definition ---- */
typedef struct {
    uint16_t warm_pwm;
    uint16_t cool_pwm;
    uint8_t  brightness;
    uint16_t cct_target;
} scene_target_t;

/* ---- Get circadian target for a given hour (0-23) + minute (0-59) ---- */
scene_target_t circadian_get_target(int hour, int minute)
{
    scene_target_t t = {0, 0, 0, 2700};
    float time_h = hour + minute / 60.0f;

    if (time_h >= 6.0f && time_h < 9.0f) {
        /* Sunrise: warm→cool, dim→bright */
        float progress = (time_h - 6.0f) / 3.0f;  /* 0→1 */
        t.warm_pwm = (uint16_t)(255 * (1.0f - progress * 0.7f));
        t.cool_pwm = (uint16_t)(255 * progress);
        t.brightness = (uint8_t)(10 + progress * 70);
        t.cct_target = (uint16_t)(2700 + progress * 2800);
    }
    else if (time_h >= 9.0f && time_h < 12.0f) {
        /* Morning work: cool, bright */
        t.warm_pwm = 32;
        t.cool_pwm = 255;
        t.brightness = 90;
        t.cct_target = 5500;
    }
    else if (time_h >= 12.0f && time_h < 17.0f) {
        /* Afternoon: cool, slightly dimmer */
        t.warm_pwm = 48;
        t.cool_pwm = 240;
        t.brightness = 80;
        t.cct_target = 5000;
    }
    else if (time_h >= 17.0f && time_h < 20.0f) {
        /* Sunset: cool→warm, bright→dim */
        float progress = (time_h - 17.0f) / 3.0f;
        t.warm_pwm = (uint16_t)(64 + progress * 191);
        t.cool_pwm = (uint16_t)(240 * (1.0f - progress));
        t.brightness = (uint8_t)(80 - progress * 40);
        t.cct_target = (uint16_t)(5000 - progress * 2300);
    }
    else if (time_h >= 20.0f && time_h < 22.0f) {
        /* Evening: warm, dim */
        float progress = (time_h - 20.0f) / 2.0f;
        t.warm_pwm = 200;
        t.cool_pwm = 16;
        t.brightness = (uint8_t)(30 - progress * 20);
        t.cct_target = 2700;
    }
    else {
        /* Night: off or very dim warm */
        t.warm_pwm = 10;
        t.cool_pwm = 0;
        t.brightness = 3;
        t.cct_target = 2200;
    }
    return t;
}

/* ---- De-stress scene target (adjusts by stress level) ---- */
scene_target_t destress_get_target(uint8_t stress_score)
{
    scene_target_t t;
    /* Higher stress → warmer, dimmer */
    if (stress_score >= 70) {
        t.warm_pwm = 255;
        t.cool_pwm = 16;
        t.brightness = 30;
        t.cct_target = 2700;
    } else if (stress_score >= 50) {
        t.warm_pwm = 200;
        t.cool_pwm = 48;
        t.brightness = 50;
        t.cct_target = 3000;
    } else {
        t.warm_pwm = 160;
        t.cool_pwm = 96;
        t.brightness = 60;
        t.cct_target = 3500;
    }
    return t;
}

/* ---- Breathing scene: pulsing brightness synced to breath pattern ---- */
/* Returns the brightness for the given phase of the breathing cycle.
 * phase: 0=inhale, 1=hold, 2=exhale, 3=hold-empty
 * progress: 0.0-1.0 within the phase
 */
uint8_t breathing_get_brightness(uint8_t phase, float progress)
{
    switch (phase) {
    case 0:  /* inhale — brighten */
        return (uint8_t)(20 + progress * 60);  /* 20 → 80 */
    case 1:  /* hold — steady */
        return 80;
    case 2:  /* exhale — dim */
        return (uint8_t)(80 - progress * 60);  /* 80 → 20 */
    case 3:  /* hold empty — steady dim */
        return 20;
    default:
        return 40;
    }
}

/* ---- Sunrise simulation for wake-up ---- */
/* Gradual warm brightening over 30 minutes.
 * progress: 0.0 (start) → 1.0 (fully awake)
 */
scene_target_t sunrise_get_target(float progress)
{
    scene_target_t t;
    if (progress < 0.3f) {
        /* Very dim warm red (like dawn) */
        t.warm_pwm = (uint16_t)(50 * progress / 0.3f);
        t.cool_pwm = 0;
        t.brightness = (uint8_t)(5 + progress * 15);
        t.cct_target = 2200;
    } else if (progress < 0.7f) {
        /* Warm orange → yellow */
        float p = (progress - 0.3f) / 0.4f;
        t.warm_pwm = (uint16_t)(50 + p * 150);
        t.cool_pwm = (uint16_t)(p * 80);
        t.brightness = (uint8_t)(20 + p * 40);
        t.cct_target = (uint16_t)(2200 + p * 1500);
    } else {
        /* Bright cool daylight */
        float p = (progress - 0.7f) / 0.3f;
        t.warm_pwm = (uint16_t)(200 - p * 168);
        t.cool_pwm = (uint16_t)(80 + p * 175);
        t.brightness = (uint8_t)(60 + p * 30);
        t.cct_target = (uint16_t)(3700 + p * 1800);
    }
    return t;
}

/* ---- Sunset simulation for evening wind-down ---- */
scene_target_t sunset_get_target(float progress)
{
    scene_target_t t;
    /* Cool bright → warm dim over 60 minutes */
    t.warm_pwm = (uint16_t)(64 + progress * 191);
    t.cool_pwm = (uint16_t)(240 * (1.0f - progress));
    t.brightness = (uint8_t)(80 - progress * 60);
    t.cct_target = (uint16_t)(5000 - progress * 2300);
    return t;
}

/* ---- Smoothly interpolate between two scene targets ---- */
scene_target_t scene_lerp(const scene_target_t *a, const scene_target_t *b,
                          float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    scene_target_t r;
    r.warm_pwm = (uint16_t)(a->warm_pwm + (b->warm_pwm - a->warm_pwm) * t);
    r.cool_pwm = (uint16_t)(a->cool_pwm + (b->cool_pwm - a->cool_pwm) * t);
    r.brightness = (uint8_t)(a->brightness + (b->brightness - a->brightness) * t);
    r.cct_target = (uint16_t)(a->cct_target + (b->cct_target - a->cct_target) * t);
    return r;
}
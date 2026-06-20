/*
 * intervention.c — CalmGrid Hub intervention system
 *
 * Manages playback of breathing-guide audio, nature soundscapes, and
 * lighting commands for stress intervention. Uses the MAX98357A I2S
 * amplifier + 28mm speaker connected to the hub.
 *
 * Breathing patterns:
 *   0 = 4-7-8 (inhale 4s, hold 7s, exhale 8s) — relaxation
 *   1 = Box (4-4-4-4) — focus / equanimity
 *   2 = Coherent (5-5) — HRV resonance ~0.1Hz
 *
 * Audio tracks stored in W25Q256 external flash as 8kHz μ-law PCM.
 * ~30s tracks = ~240KB each.
 *
 * Intervention strategy:
 * 1. On acute stress episode → lighting de-stress scene + gentle chime
 * 2. If stress persists → start breathing guide (pattern chosen by model)
 * 3. If still high after 5 min → play nature soundscape
 * 4. Track HRV/EDA before/after to measure intervention efficacy
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include "calm_protocol.h"

/* ---- Breathing pattern definitions ---- */
typedef enum {
    BREATH_PATTERN_4_7_8  = 0,
    BREATH_PATTERN_BOX    = 1,
    BREATH_PATTERN_COHERENT = 2,
    BREATH_PATTERN_COUNT
} breath_pattern_t;

typedef struct {
    uint8_t inhale_s;
    uint8_t hold_s;
    uint8_t exhale_s;
    uint8_t hold_empty_s;
    const char *name;
} breath_pattern_meta_t;

static const breath_pattern_meta_t patterns[BREATH_PATTERN_COUNT] = {
    { 4, 7, 8, 0, "4-7-8 Relaxation" },
    { 4, 4, 4, 4, "Box Breathing" },
    { 5, 0, 5, 0, "Coherent Breathing" },
};

/* ---- Soundscape track definitions ---- */
typedef enum {
    SOUND_OCEAN       = 0,
    SOUND_RAIN        = 1,
    SOUND_FOREST      = 2,
    SOUND_STREAM      = 3,
    SOUND_PINK_NOISE  = 4,
    SOUND_COUNT
} soundscape_t;

typedef struct {
    uint32_t flash_offset;
    uint32_t length_bytes;
    const char *name;
} soundscape_meta_t;

static const soundscape_meta_t soundscapes[SOUND_COUNT] = {
    { 0x040000, 240000, "Ocean Waves" },
    { 0x07B000, 240000, "Gentle Rain" },
    { 0x0B6000, 240000, "Forest Birds" },
    { 0x0F1000, 240000, "Mountain Stream" },
    { 0x12C000, 240000, "Pink Noise" },
};

/* ---- Intervention state ---- */
typedef struct {
    uint8_t  active;
    uint8_t  type;          /* 0=breathing 1=soundscape 2=lighting 3=combined */
    uint8_t  breath_pattern;
    uint8_t  soundscape_id;
    uint16_t duration_s;
    uint16_t elapsed_s;
    /* Pre-intervention vitals (for efficacy measurement) */
    uint16_t pre_hrv;
    uint16_t pre_eda_scr;
    uint8_t  pre_hr;
} intervention_state_t;

static intervention_state_t g_intervention = {0};

/* ---- Start an intervention ---- */
void intervention_start(uint8_t type, uint8_t param1, uint8_t param2,
                        uint16_t duration_s,
                        uint16_t cur_hrv, uint16_t cur_eda_scr, uint8_t cur_hr)
{
    if (g_intervention.active) return;  /* don't interrupt ongoing */

    g_intervention.active = 1;
    g_intervention.type = type;
    g_intervention.breath_pattern = (type == 0 || type == 3) ? param1 : 0;
    g_intervention.soundscape_id = (type == 1 || type == 3) ? param2 : 0;
    g_intervention.duration_s = duration_s;
    g_intervention.elapsed_s = 0;
    g_intervention.pre_hrv = cur_hrv;
    g_intervention.pre_eda_scr = cur_eda_scr;
    g_intervention.pre_hr = cur_hr;

    /* Trigger lighting if combined */
    if (type == 2 || type == 3) {
        calm_mesh_send_lighting(CALM_SCENE_DESTRESS, 40, 2700, 0);
    }
}

/* ---- Update intervention (called every second) ---- */
typedef struct {
    uint8_t  intervention_type;
    uint8_t  breath_pattern;
    uint8_t  soundscape_id;
    uint16_t duration_s;
    uint16_t elapsed_s;
    int16_t  hrv_delta;   /* post - pre (positive = improvement) */
    int16_t  eda_delta;   /* post - pre (negative = improvement) */
    int16_t  hr_delta;
    uint8_t  efficacy;    /* 0-100 (computed from HRV improvement) */
} intervention_result_t;

intervention_result_t g_last_result = {0};

int intervention_update(uint16_t cur_hrv, uint16_t cur_eda_scr, uint8_t cur_hr)
{
    if (!g_intervention.active) return 0;

    g_intervention.elapsed_s++;

    if (g_intervention.elapsed_s >= g_intervention.duration_s) {
        /* Intervention complete — compute efficacy */
        g_last_result.intervention_type = g_intervention.type;
        g_last_result.breath_pattern = g_intervention.breath_pattern;
        g_last_result.soundscape_id = g_intervention.soundscape_id;
        g_last_result.duration_s = g_intervention.duration_s;
        g_last_result.elapsed_s = g_intervention.elapsed_s;
        g_last_result.hrv_delta = (int16_t)cur_hrv - (int16_t)g_intervention.pre_hrv;
        g_last_result.eda_delta = (int16_t)cur_eda_scr - (int16_t)g_intervention.pre_eda_scr;
        g_last_result.hr_delta  = (int16_t)cur_hr - (int16_t)g_intervention.pre_hr;

        /* Efficacy: HRV improvement is the primary metric */
        float eff = 0;
        if (g_intervention.pre_hrv > 0) {
            eff = (float)g_last_result.hrv_delta / (float)g_intervention.pre_hrv * 100.0f;
            if (eff < 0) eff = 0;
            if (eff > 100) eff = 100;
        }
        g_last_result.efficacy = (uint8_t)eff;

        g_intervention.active = 0;
        return 1;  /* completed */
    }

    return 0;  /* still running */
}

/* ---- Get breathing phase for animation ---- */
/* Returns phase: 0=inhale, 1=hold, 2=exhale, 3=hold-empty
 * And phase progress 0.0-1.0 for the animation */
void intervention_get_breath_phase(uint8_t *phase, float *progress)
{
    if (!g_intervention.active || g_intervention.type != 0) {
        *phase = 0;
        *progress = 0;
        return;
    }

    const breath_pattern_meta_t *p = &patterns[g_intervention.breath_pattern];
    uint16_t cycle_len = p->inhale_s + p->hold_s + p->exhale_s + p->hold_empty_s;
    uint16_t cycle_pos = g_intervention.elapsed_s % cycle_len;

    if (cycle_pos < p->inhale_s) {
        *phase = 0;  /* inhale */
        *progress = (float)cycle_pos / (float)p->inhale_s;
    } else if (cycle_pos < p->inhale_s + p->hold_s) {
        *phase = 1;  /* hold */
        *progress = (float)(cycle_pos - p->inhale_s) / (float)p->hold_s;
    } else if (cycle_pos < p->inhale_s + p->hold_s + p->exhale_s) {
        *phase = 2;  /* exhale */
        *progress = (float)(cycle_pos - p->inhale_s - p->hold_s) /
                    (float)p->exhale_s;
    } else {
        *phase = 3;  /* hold empty */
        *progress = (float)(cycle_pos - p->inhale_s - p->hold_s - p->exhale_s) /
                    (float)p->hold_empty_s;
    }
}

/* ---- Audio playback stub (I2S → MAX98357A) ---- */
/* In production, this reads μ-law PCM from flash and plays via I2S */
void intervention_play_breath_chime(uint8_t phase)
{
    /* phase 0 = rising tone (inhale), phase 2 = falling tone (exhale) */
    (void)phase;
}

void intervention_play_soundscape(uint8_t track_id)
{
    if (track_id >= SOUND_COUNT) return;
    /* Play soundscape from flash offset */
    (void)track_id;
}

void intervention_stop_audio(void)
{
    /* Stop I2S playback */
}

/* ---- Get last intervention result ---- */
const intervention_result_t *intervention_get_last_result(void)
{
    return &g_last_result;
}

/* ---- Check if intervention is active ---- */
int intervention_is_active(void)
{
    return g_intervention.active;
}
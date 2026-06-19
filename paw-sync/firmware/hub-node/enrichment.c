/*
 * enrichment.c — PawSync Hub enrichment audio system
 *
 * Manages playback of calming audio cues for separation anxiety management.
 * Uses the MAX98357A I2S amplifier + 28mm speaker connected to the hub.
 *
 * Audio tracks are stored in the W25Q256 external flash, pre-encoded as
 * 8kHz μ-law PCM (compact, ~30s tracks = ~240KB each).
 *
 * Enrichment strategy:
 * 1. On anxiety episode detection → play calming audio (classical/frequency)
 * 2. If anxiety persists → dispense treat via feeder
 * 3. If still anxious → play owner voice recording
 * 4. Track which interventions reduce anxiety (learned over time)
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include "paw_protocol.h"

/* ---- Enrichment track definitions ---- */
typedef enum {
    ENRICH_TRACK_CALM_BEDTIME  = 0,   /* low-frequency lullaby */
    ENRICH_TRACK_CLASSICAL      = 1,   /* classical music (proven calming) */
    ENRICH_TRACK_WHITE_NOISE    = 2,   /* white/pink noise */
    ENRICH_TRACK_OWNER_VOICE    = 3,   /* owner's voice recording */
    ENRICH_TRACK_TREAT_CUE      = 4,   /* sound that signals treat */
    ENRICH_TRACK_COUNT
} enrich_track_t;

/* Track metadata: offset + length in external flash */
typedef struct {
    uint32_t flash_offset;
    uint32_t length_bytes;
    const char *name;
} enrich_track_meta_t;

static const enrich_track_meta_t tracks[ENRICH_TRACK_COUNT] = {
    { 0x000000, 240000, "Calm Bedtime" },
    { 0x03C000, 240000, "Classical" },
    { 0x078000, 240000, "White Noise" },
    { 0x0B4000, 120000, "Owner Voice" },
    { 0x0D2000,  30000, "Treat Cue" },
};

/* ---- Intervention history (for learning) ---- */
#define MAX_INTERVENTIONS 32
typedef struct {
    uint32_t timestamp;
    uint8_t  track_id;
    uint8_t  anxiety_before;  /* 0-100 */
    uint8_t  anxiety_after;   /* 0-100 (measured 10 min later) */
    uint8_t  effective;       /* anxiety_after < anxiety_before */
} intervention_record_t;

static intervention_record_t history[MAX_INTERVENTIONS];
static int history_count = 0;
static int current_track = -1;
static int current_volume = 50;

/* ---- Effectiveness tracking ---- */
static int track_effectiveness[ENRICH_TRACK_COUNT] = {0};
static int track_uses[ENRICH_TRACK_COUNT] = {0};

/* ---- I2S audio interface (stub — in production: MAX98357A driver) ---- */
static void i2s_init(void) {}
static void i2s_write_pcm(const uint8_t *data, size_t len, int volume) {
    (void)data; (void)len; (void)volume;
}
static void flash_read(uint32_t offset, uint8_t *buf, uint32_t len) {
    (void)offset; memset(buf, 0, len);  /* stub: silence */
}

/* ---- Track playback ---- */
void enrichment_play_track(enrich_track_t track, int volume)
{
    if (track >= ENRICH_TRACK_COUNT) return;
    current_track = track;
    current_volume = volume;

    const enrich_track_meta_t *t = &tracks[track];
    uint8_t buf[1024];
    uint32_t remaining = t->length_bytes;
    uint32_t offset = t->flash_offset;

    /* Stream from flash in 1KB chunks */
    while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        flash_read(offset, buf, chunk);
        /* Convert μ-law to PCM and apply volume */
        i2s_write_pcm(buf, chunk, volume);
        offset += chunk;
        remaining -= chunk;
    }
    current_track = -1;
}

/* ---- Select best track based on learned effectiveness ---- */
static enrich_track_t select_best_track(void)
{
    enrich_track_t best = ENRICH_TRACK_CLASSICAL;  /* default */
    int best_score = -1;
    for (int i = 0; i < ENRICH_TRACK_COUNT; i++) {
        if (track_uses[i] < 3) continue;  /* need minimum samples */
        int score = track_effectiveness[i] * 100 / track_uses[i];
        if (score > best_score) {
            best_score = score;
            best = (enrich_track_t)i;
        }
    }
    return best;
}

/* ---- Main enrichment trigger ---- */
void enrichment_trigger(uint8_t anxiety_level)
{
    /* Strategy: escalate based on anxiety severity */
    if (anxiety_level < 40) {
        /* Mild: just play calming track */
        enrichment_play_track(ENRICH_TRACK_CALM_BEDTIME, 30);
    } else if (anxiety_level < 70) {
        /* Moderate: play learned-best track at medium volume */
        enrich_track_t t = select_best_track();
        enrichment_play_track(t, 50);
    } else {
        /* Severe: play owner voice at high volume */
        enrichment_play_track(ENRICH_TRACK_OWNER_VOICE, 70);
    }
}

/* ---- Record intervention outcome ---- */
void enrichment_record_outcome(uint8_t track_id, uint8_t anxiety_before,
                                uint8_t anxiety_after)
{
    if (history_count < MAX_INTERVENTIONS) {
        history[history_count].timestamp = 0;  /* TODO: RTC */
        history[history_count].track_id = track_id;
        history[history_count].anxiety_before = anxiety_before;
        history[history_count].anxiety_after = anxiety_after;
        history[history_count].effective = (anxiety_after < anxiety_before) ? 1 : 0;
        history_count++;
    }

    /* Update effectiveness counters */
    if (track_id < ENRICH_TRACK_COUNT) {
        track_uses[track_id]++;
        if (anxiety_after < anxiety_before)
            track_effectiveness[track_id]++;
    }
}

/* ---- Treat cue (for feeder coordination) ---- */
void enrichment_play_treat_cue(void)
{
    enrichment_play_track(ENRICH_TRACK_TREAT_CUE, 60);
}

/* ---- Get current playing status ---- */
int enrichment_is_playing(void)
{
    return (current_track >= 0) ? 1 : 0;
}

int enrichment_current_track(void)
{
    return current_track;
}
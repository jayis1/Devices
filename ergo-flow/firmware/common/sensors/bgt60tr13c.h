/*
 * ErgoFlow — BGT60TR13C mmWave Radar Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef BGT60TR13C_H
#define BGT60TR13C_H

#include <stdint.h>
#include <stdbool.h>

#define BGT60_MAX_SAMPLES  1024

typedef struct {
    uint16_t chirps_per_frame;
    uint16_t samples_per_chirp;
    uint16_t frame_rate_hz;
    float start_freq_ghz;
    float bandwidth_ghz;
} bgt60_config_t;

typedef struct {
    int32_t raw_data[BGT60_MAX_SAMPLES];  /* I/Q interleaved */
    uint16_t num_chirps;
    uint16_t num_samples;
    uint32_t timestamp;
} bgt60_radar_frame_t;

int bgt60tr13c_init(void);
int bgt60tr13c_start(void);
int bgt60tr13c_stop(void);
int bgt60tr13c_read_frame(bgt60_radar_frame_t *frame);
int bgt60tr13c_get_config(bgt60_config_t *config);
int bgt60tr13c_set_config(const bgt60_config_t *config);

#endif /* BGT60TR13C_H */
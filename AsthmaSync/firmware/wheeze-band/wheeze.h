/**
 * AsthmaSync — Wheeze Band Audio Header
 *
 * License: MIT
 */

#ifndef WHEEZE_H
#define WHEEZE_H

#include "../common/protocol.h"
#include <stdint.h>
#include <stddef.h>

/** Callback type for wheeze detection events. */
typedef void (*wheeze_callback_t)(uint8_t probability, const audio_feature_t *feature);

/** Initialize wheeze detection (mel filterbank, Hamming window). */
int wheeze_init(void);

/** Process a 2-second audio frame.
 *  @param audio         16-bit PCM samples (16 kHz)
 *  @param samples       number of samples (must be AUDIO_FRAME_SAMPLES)
 *  @param out_wheeze_prob  pre-classifier probability (0-100)
 *  @param out_feature   audio feature for Hub CNN (may be NULL)
 *  @return 0 on success */
int wheeze_process_frame(const int16_t *audio, size_t samples,
                         uint8_t *out_wheeze_prob, audio_feature_t *out_feature);

/** Called by I²S DMA handler when audio samples are available. */
void wheeze_on_audio(const int16_t *samples, size_t count);

/** Register callback for wheeze detection events. */
void wheeze_set_callback(wheeze_callback_t cb);

/** Get wheeze event count since boot. */
uint8_t wheeze_get_count(void);

#endif /* WHEEZE_H */
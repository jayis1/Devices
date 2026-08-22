#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"

typedef struct {
    float rgb_score;
    float spectral_score;
    bool barcode_present;
    uint16_t mass_grams;
} sorter_sample_t;

static uint8_t choose_stream(const sorter_sample_t *sample) {
    float score = (sample->rgb_score * 0.55f) + (sample->spectral_score * 0.45f);
    if (sample->barcode_present && score > 0.82f) {
        return WS_STREAM_RECYCLE;
    }
    if (score > 0.58f && sample->mass_grams > 40u) {
        return WS_STREAM_COMPOST;
    }
    return WS_STREAM_LANDFILL;
}

static uint16_t confidence_q15(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 0.9999f) value = 0.9999f;
    return (uint16_t)lrintf(value * 32767.0f);
}

int main(void) {
    sorter_sample_t samples[] = {
        {0.91f, 0.83f, true, 18u},
        {0.42f, 0.71f, false, 74u},
        {0.35f, 0.29f, false, 11u},
    };

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        uint8_t stream = choose_stream(&samples[i]);
        ws_sort_event_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.item_id = (uint32_t)(1000u + i);
        evt.recommended_stream = stream;
        evt.material_class = (uint8_t)(i + 1u);
        evt.confidence_q15 = confidence_q15((samples[i].rgb_score + samples[i].spectral_score) * 0.5f);
        evt.contamination_risk_pct = (uint8_t)(100u - (evt.confidence_q15 / 328u));
        evt.barcode_present = samples[i].barcode_present ? 1u : 0u;
        evt.mass_grams = samples[i].mass_grams;
        printf("sorter item=%u stream=%u conf=%u contam=%u mass=%u\n",
               evt.item_id,
               evt.recommended_stream,
               evt.confidence_q15,
               evt.contamination_risk_pct,
               evt.mass_grams);
    }

    return 0;
}

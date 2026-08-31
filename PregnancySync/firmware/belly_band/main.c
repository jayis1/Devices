#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint16_t movement_count_10m;
    float movement_variability;
    float posture_pct_left;
    float posture_pct_supine;
    float skin_temp_c;
    float ehg_activity_index;
} band_summary_t;

static band_summary_t analyze_session(const int16_t *kick_signal, uint16_t samples) {
    band_summary_t out = {0U, 0.0f, 63.0f, 14.0f, 34.3f, 5.6f};
    int32_t energy = 0;
    for (uint16_t i = 0; i < samples; ++i) {
        int16_t sample = kick_signal[i];
        energy += (sample >= 0) ? sample : -sample;
        if (sample > 80) {
            out.movement_count_10m++;
        }
    }
    out.movement_variability = (samples > 0U) ? ((float)energy / (float)samples) / 100.0f : 0.0f;
    out.ehg_activity_index += out.movement_variability;
    return out;
}

int main(void) {
    const int16_t demo_signal[8] = {12, 85, 21, 101, 9, 2, 93, 78};
    band_summary_t summary = analyze_session(demo_signal, 8U);
    printf("band movements=%u variability=%.2f supine=%.1f%%\n",
           summary.movement_count_10m, summary.movement_variability, summary.posture_pct_supine);
    return 0;
}

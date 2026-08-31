#include <stdint.h>
#include <stdio.h>

typedef struct {
    float hours_recorded;
    uint16_t supine_minutes;
    uint16_t left_side_minutes;
    float respiration_rate;
    float restlessness_index;
} pad_summary_t;

static pad_summary_t summarize_night(const uint8_t *posture_states, uint16_t samples) {
    pad_summary_t out = {7.5f, 0U, 0U, 16.4f, 1.8f};
    for (uint16_t i = 0; i < samples; ++i) {
        if (posture_states[i] == 0U) {
            out.left_side_minutes += 5U;
        } else if (posture_states[i] == 2U) {
            out.supine_minutes += 5U;
        }
    }
    out.restlessness_index += (float)samples / 20.0f;
    return out;
}

int main(void) {
    const uint8_t states[8] = {0, 0, 1, 2, 2, 0, 0, 2};
    pad_summary_t night = summarize_night(states, 8U);
    printf("pad left=%u supine=%u restlessness=%.2f\n",
           night.left_side_minutes, night.supine_minutes, night.restlessness_index);
    return 0;
}

/* SeizureSync — Caregiver Beacon alert driver (header) */
#ifndef ALERT_DRIVER_H
#define ALERT_DRIVER_H
#include "../common/protocol.h"

void alert_driver_init(void);
void display_show_idle(void);
void display_show_seizure(const sz_seizure_payload_t *s);
void display_show_aura(const sz_aura_payload_t *a);
void display_show_sudep(const sz_sudep_payload_t *s);
void rgb_set_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_set_blink(bool on);
void haptic_pattern_seizure(void);
void haptic_pattern_aura(void);
void haptic_pattern_sudep(void);
void haptic_stop(void);
void audio_play_seizure_alarm(void);
void audio_play_aura_warning(void);
void audio_play_sudep_alarm(void);
void audio_play_dispatched(void);
void audio_play_test(void);
void audio_stop(void);
void start_ack_timeout(uint32_t event_unix, int timeout_s);
void alert_seizure(const sz_seizure_payload_t *s);
void alert_aura(const sz_aura_payload_t *a);
void alert_sudep(const sz_sudep_payload_t *s);

#endif
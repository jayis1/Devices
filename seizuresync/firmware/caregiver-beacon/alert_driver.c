/* SeizureSync — Caregiver Beacon alert driver (stubs) */
#include "alert_driver.h"
#include "esp_log.h"
#include <stdio.h>
static const char *TAG = "ALERT";

void alert_driver_init(void) {
    ESP_LOGI(TAG, "Alert drivers init (DRV2605L + MAX98357A + WS2812 + e-ink)");
}
void display_show_idle(void) {
    ESP_LOGI(TAG, "E-ink: idle — SeizureSync Beacon Ready");
}
void display_show_seizure(const sz_seizure_payload_t *s) {
    ESP_LOGW(TAG, "E-ink: SEIZURE type=%d conf=%d%%", s->semiology, s->confidence);
}
void display_show_aura(const sz_aura_payload_t *a) {
    ESP_LOGW(TAG, "E-ink: AURA lead=%ds prob=%d%%", a->lead_time_s, a->probability);
}
void display_show_sudep(const sz_sudep_payload_t *s) {
    ESP_LOGW(TAG, "E-ink: SUDEP apnea=%ds spo2=%d%%", s->apnea_duration_s, s->spo2_pct);
}
void rgb_set_color(uint8_t r, uint8_t g, uint8_t b) {
    ESP_LOGI(TAG, "RGB: #%02x%02x%02x", r, g, b);
}
void rgb_set_blink(bool on) { ESP_LOGI(TAG, "RGB blink=%d", on); }
void haptic_pattern_seizure(void) { ESP_LOGW(TAG, "Haptic: seizure (3-burst)"); }
void haptic_pattern_aura(void)   { ESP_LOGW(TAG, "Haptic: aura (double-pulse)"); }
void haptic_pattern_sudep(void)   { ESP_LOGW(TAG, "Haptic: SUDEP (continuous)"); }
void haptic_stop(void)            { ESP_LOGI(TAG, "Haptic stop"); }
void audio_play_seizure_alarm(void)  { ESP_LOGW(TAG, "Audio: seizure alarm 85dB"); }
void audio_play_aura_warning(void)  { ESP_LOGI(TAG, "Audio: aura warning"); }
void audio_play_sudep_alarm(void)    { ESP_LOGW(TAG, "Audio: SUDEP alarm (max)"); }
void audio_play_dispatched(void)     { ESP_LOGI(TAG, "Audio: 911 dispatched"); }
void audio_play_test(void)           { ESP_LOGI(TAG, "Audio: test tone"); }
void audio_stop(void)               { ESP_LOGI(TAG, "Audio stop"); }
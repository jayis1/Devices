/**
 * Alert engine — progressive glucose alerts.
 * License: MIT
 */

#include "alert_engine.h"
#include <string.h>

static uint8_t g_current_level = 0;

void alert_engine_init(void)
{
    g_current_level = 0;
}

uint8_t alert_engine_evaluate(uint8_t risk_score, uint16_t forecast_glucose, uint8_t hypo_risk)
{
    /* Critical: predicted glucose <54 or hypo_risk >85 */
    if (forecast_glucose < 54 || hypo_risk > 85 || risk_score >= 80) {
        return 4;
    }
    /* High: predicted glucose <70 or hypo_risk >60 */
    if (forecast_glucose < 70 || hypo_risk > 60 || risk_score >= 60) {
        return 3;
    }
    /* Moderate: hypo_risk >40 or risk >40 */
    if (hypo_risk > 40 || risk_score >= 40) {
        return 2;
    }
    /* Low: risk >20 */
    if (risk_score >= 20) {
        return 1;
    }
    return 0;
}

void alert_engine_trigger(uint8_t level, uint8_t recommendation)
{
    g_current_level = level;

    switch (level) {
    case 1:
        alert_play_sound(ALERT_LOW_CHIME);
        break;
    case 2:
        alert_play_sound(ALERT_MODERATE);
        if (recommendation == 2) {
            alert_play_voice("Consider 15 grams of fast-acting carbs.");
        }
        break;
    case 3:
        alert_play_sound(ALERT_HIGH);
        if (recommendation == 2) {
            alert_play_voice("Low glucose predicted. Eat 15 grams of carbs now.");
        } else if (recommendation == 3) {
            alert_play_voice("High glucose predicted. Consider insulin correction.");
        }
        break;
    case 4:
        alert_play_sound(ALERT_CRITICAL);
        if (recommendation == 2) {
            alert_play_voice("Critical low glucose predicted. Take action immediately.");
        } else if (recommendation == 5) {
            alert_play_voice("Critical glucose level. Seek help immediately.");
        }
        break;
    default:
        break;
    }
}

void alert_engine_dismiss(void)
{
    g_current_level = 0;
    /* Production: stop haptic, stop audio */
}

void alert_play_sound(alert_sound_t sound)
{
    (void)sound;
    /* Production: play tone via MAX98357A I²S amplifier
     * - LOW_CHIME: single 880 Hz beep, 200ms
     * - MODERATE: two-tone 660→880 Hz, 300ms
     * - HIGH: three-tone 440→660→880 Hz, 500ms
     * - CRITICAL: continuous 1000 Hz siren, until dismissed */
}

void alert_play_voice(const char *message)
{
    (void)message;
    /* Production: synthesize via esp_tts or play pre-recorded WAV from flash */
}
#ifndef GLUCOSYNC_ALERT_ENGINE_H
#define GLUCOSYNC_ALERT_ENGINE_H

#include <stdint.h>

/**
 * Progressive alert engine.
 * Levels: 0=none, 1=low, 2=moderate, 3=high, 4=critical
 */

void alert_engine_init(void);
uint8_t alert_engine_evaluate(uint8_t risk_score, uint16_t forecast_glucose, uint8_t hypo_risk);
void alert_engine_trigger(uint8_t level, uint8_t recommendation);
void alert_engine_dismiss(void);

/* Alert sound types (passed to speaker) */
typedef enum {
    ALERT_NONE = 0,
    ALERT_LOW_CHIME = 1,
    ALERT_MODERATE = 2,
    ALERT_HIGH = 3,
    ALERT_CRITICAL = 4,
} alert_sound_t;

void alert_play_sound(alert_sound_t sound);
void alert_play_voice(const char *message);

#endif
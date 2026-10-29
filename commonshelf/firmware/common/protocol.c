#include "protocol.h"
int cs_valid(const cs_sample_t *s) {
 if (!s || s->battery_mv > 6000u) return 0;
 switch (s->kind) {
 case CS_MASS_G: return s->value >= 0 && s->value <= 10000;
 case CS_TEMP_C_X100: return s->value >= -5000 && s->value <= 10000;
 case CS_DOOR: return s->value == 0 || s->value == 1;
 default: return 0;
 }
}

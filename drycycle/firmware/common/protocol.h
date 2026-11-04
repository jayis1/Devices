#ifndef DRYCYCLE_PROTOCOL_H
#define DRYCYCLE_PROTOCOL_H
#include <stdint.h>
#define DC_PROTOCOL_VERSION 1u
#define DC_MAX_PAYLOAD 192u
typedef enum { DC_MASS_G=1, DC_RH_BPS=2, DC_TEMP_CENTI_C=3 } dc_kind_t;
typedef struct { uint8_t version; uint32_t seq; uint8_t kind; int32_t value; uint64_t utc_ms; } dc_event_t;
/* Validate decoded event values; wire parser and BLE authentication are not implemented. */
static inline int dc_event_valid(const dc_event_t *e) {
    if (!e || e->version != DC_PROTOCOL_VERSION) return 0;
    if (e->kind == DC_MASS_G) return e->value >= 0 && e->value <= 50000;
    if (e->kind == DC_RH_BPS) return e->value >= 0 && e->value <= 10000;
    if (e->kind == DC_TEMP_CENTI_C) return e->value >= -4000 && e->value <= 8500;
    return 0;
}
#endif

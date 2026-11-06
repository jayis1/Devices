#ifndef ARCHIVEGUARD_PROTOCOL_H
#define ARCHIVEGUARD_PROTOCOL_H
#include <stdint.h>
struct telemetry { uint8_t version; uint16_t node_id; uint32_t seq; float temperature_c; float rh_pct; };
static inline int telemetry_valid(const struct telemetry *t) {
    return t != 0 && t->version == 1 && t->node_id != 0 && t->temperature_c >= -20.0f && t->temperature_c <= 60.0f && t->rh_pct >= 0.0f && t->rh_pct <= 100.0f;
}
#endif

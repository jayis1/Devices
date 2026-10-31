#include <stdint.h>
#include <stddef.h>

#define COURTSIDE_PROTOCOL_VERSION 1u
#define COURTSIDE_MAX_FRAME 512u

typedef enum {
    COURTSIDE_SURFACE_OBSERVATION = 1,
    COURTSIDE_INSPECTION = 2,
    COURTSIDE_MAINTENANCE_REPORT = 3
} courtside_kind_t;

typedef struct {
    uint8_t version;
    uint32_t sequence;
    uint8_t kind;
    int32_t wetness_raw;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_pct;
} courtside_sample_t;

/* Fixed-width binary payload helper for host-side tests; radio transport is GATT. */
size_t courtside_encode(const courtside_sample_t *sample, uint8_t *out, size_t capacity);
int courtside_decode(const uint8_t *data, size_t length, courtside_sample_t *out);

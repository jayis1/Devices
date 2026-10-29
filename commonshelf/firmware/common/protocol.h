#ifndef COMMONSHELF_PROTOCOL_H
#define COMMONSHELF_PROTOCOL_H
#include <stdint.h>
#define CS_VERSION 1u
#define CS_MAX_PAYLOAD 160u
/* Wire format is JSON; never transmit this C struct as bytes. */
typedef enum { CS_MASS_G=1, CS_TEMP_C_X100=2, CS_DOOR=3 } cs_kind_t;
typedef struct { uint32_t seq; uint64_t timestamp_s; int32_t value; uint16_t battery_mv; cs_kind_t kind; } cs_sample_t;
int cs_valid(const cs_sample_t *s);
#endif

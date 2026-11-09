#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RW_MAX_BAYS 8u
#define RW_VERSION 1u

typedef enum { RW_UNKNOWN = 0, RW_CLEAR = 1, RW_OCCUPIED = 2 } rw_state_t;
typedef struct { uint32_t seq; uint8_t count; rw_state_t bay[RW_MAX_BAYS]; } rw_rack_report_t;

/* Fixed-length host/firmware shared payload: version, count, seq LE, bay states. */
size_t rw_encode(const rw_rack_report_t *in, uint8_t *out, size_t cap);
bool rw_decode(const uint8_t *buf, size_t len, rw_rack_report_t *out);

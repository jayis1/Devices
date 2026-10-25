#include <stdint.h>
#define CU_VERSION 1u
#define CU_MAX_EVENTS 10u
typedef enum { CU_OBSTRUCTION=1, CU_WET=2, CU_TILT=3, CU_MANUAL=4, CU_ACK=5 } cu_kind_t;
typedef struct { uint32_t seq; uint32_t sample_ms; uint8_t kind; uint8_t severity; } cu_event_t;
/* Portable bounded prototype validation; BLE encoding/authentication is not implemented. */
int cu_event_valid(const cu_event_t *e);

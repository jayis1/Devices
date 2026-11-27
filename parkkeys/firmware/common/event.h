#include <stdint.h>

#define PARKKEYS_PROTOCOL_VERSION 1
#define PARKKEYS_MAX_PAYLOAD 512

typedef enum { REPORT_OK, REPORT_CLEANING, REPORT_REPAIR } piano_report_t;

typedef struct {
    uint32_t sequence;
    piano_report_t report;
} parkkeys_event_t;

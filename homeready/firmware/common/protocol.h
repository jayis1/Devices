#ifndef HOMEREADY_PROTOCOL_H
#define HOMEREADY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define HR_MAX_EVENT_BYTES 384u
int hr_event_envelope_valid(const uint8_t *data, size_t length);
#endif

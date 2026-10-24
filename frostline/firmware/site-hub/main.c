#include "hub.h"
int frostline_hub_accept(const uint8_t *frame,size_t n,fl_telemetry_t *out){ return fl_decode(frame,n,out); }

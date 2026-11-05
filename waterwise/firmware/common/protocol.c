#include "protocol.h"
#include <math.h>
#include <string.h>
int ww_valid(const ww_telemetry_t *m) {
  if (!m || m->version != WW_PROTOCOL_VERSION || !m->node_id[0] || !isfinite(m->value)) return 0;
  if (!strcmp(m->kind,"flow")) return !strcmp(m->unit,"L/min") && m->value >= 0.0f && m->value <= 40.0f;
  if (!strcmp(m->kind,"pipe_temp")) return !strcmp(m->unit,"C") && m->value >= -30.0f && m->value <= 125.0f;
  return 0;
}

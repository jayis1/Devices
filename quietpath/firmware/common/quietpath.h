/* QuietPath protocol reference */
#ifndef QUIETPATH_H
#define QUIETPATH_H
#include <stddef.h>
#include <stdint.h>
#define QP_FRAME_LEN 19u
#define QP_VERSION 1u
#define QP_KIND_TELEMETRY 1u
typedef struct { uint16_t node_id, sequence; uint32_t epoch_minutes; uint16_t leq_cdb, peak_cdb; uint8_t flags; } qp_report_t;
uint16_t qp_crc16(const uint8_t *data, size_t len);
int qp_encode(const qp_report_t *r, uint8_t out[QP_FRAME_LEN]);
int qp_decode(const uint8_t *in, size_t len, qp_report_t *r);
#endif

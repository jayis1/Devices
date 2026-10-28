#include "quietpath.h"
uint16_t qp_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xffffu;
    size_t i; unsigned bit;
    if (!data && len) return 0;
    for (i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; ++bit) {
            uint16_t shifted = (uint16_t)(crc << 1);
            crc = (crc & 0x8000u) ? (uint16_t)(shifted ^ 0x1021u) : shifted;
        }
    }
    return crc;
}
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
int qp_encode(const qp_report_t *r, uint8_t out[QP_FRAME_LEN]) {
    uint16_t crc;
    if (!r || !out || (r->flags & 0xfcu) ||
        ((r->leq_cdb > 14000u) && (r->leq_cdb != 0xffffu)) ||
        ((r->peak_cdb > 14000u) && (r->peak_cdb != 0xffffu))) return -1;
    out[0]=QP_VERSION; out[1]=QP_KIND_TELEMETRY; put16(out+2,r->node_id); put16(out+4,r->sequence); put32(out+6,r->epoch_minutes); put16(out+10,r->leq_cdb); put16(out+12,r->peak_cdb); out[14]=r->flags; out[15]=0; out[16]=0; crc=qp_crc16(out,17); put16(out+17,crc); return 0;
}
int qp_decode(const uint8_t *in, size_t len, qp_report_t *r) {
    if (!in || !r || len != QP_FRAME_LEN || in[0]!=QP_VERSION || in[1]!=QP_KIND_TELEMETRY || in[15]!=0 || in[16]!=0 || (in[14]&0xfcu) || get16(in+17)!=qp_crc16(in,17)) return -1;
    r->node_id=get16(in+2); r->sequence=get16(in+4); r->epoch_minutes=get32(in+6); r->leq_cdb=get16(in+10); r->peak_cdb=get16(in+12); r->flags=in[14];
    if ((r->leq_cdb>14000u && r->leq_cdb!=0xffffu) || (r->peak_cdb>14000u && r->peak_cdb!=0xffffu)) return -1;
    return 0;
}

#include "protocol.h"
int mc_encode(const mc_frame_t *f, uint8_t out[MC_FRAME_SIZE]) {
 if (!f || !out || f->version != MC_VERSION || (f->kind != MC_PROBE && f->kind != MC_DOOR)) return -1;
 if ((f->kind == MC_PROBE && (f->temp_centi_c == MC_TEMP_INVALID || f->door_state != 0xff)) || (f->kind == MC_DOOR && (f->temp_centi_c != MC_TEMP_INVALID || f->door_state > 1))) return -1;
 out[0]=f->version; out[1]=f->kind; out[2]=(uint8_t)f->sequence; out[3]=(uint8_t)(f->sequence>>8);
 out[4]=(uint8_t)f->temp_centi_c; out[5]=(uint8_t)((uint16_t)f->temp_centi_c>>8); out[6]=f->door_state;
 out[7]=(uint8_t)f->battery_mv; out[8]=(uint8_t)(f->battery_mv>>8); return 0;
}
int mc_decode(const uint8_t *d, size_t n, mc_frame_t *o) {
 if (!d || !o || n != MC_FRAME_SIZE || d[0] != MC_VERSION) return -1;
 o->version=d[0]; o->kind=d[1]; o->sequence=(uint16_t)d[2]|((uint16_t)d[3]<<8);
 o->temp_centi_c=(int16_t)((uint16_t)d[4]|((uint16_t)d[5]<<8)); o->door_state=d[6]; o->battery_mv=(uint16_t)d[7]|((uint16_t)d[8]<<8);
 if ((o->kind==MC_PROBE && (o->temp_centi_c==MC_TEMP_INVALID || o->door_state!=0xff)) || (o->kind==MC_DOOR && (o->temp_centi_c!=MC_TEMP_INVALID || o->door_state>1)) || (o->kind!=MC_PROBE && o->kind!=MC_DOOR)) return -1;
 return 0;
}

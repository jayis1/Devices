#include "protocol.h"
static void p16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint16_t g16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static void p32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t g32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int ss_encode(const ss_record_t *r,uint8_t o[SS_FRAME_SIZE]){if(!r||!o||r->version!=SS_VERSION||r->type!=SS_TYPE_TELEMETRY)return -1;o[0]=r->version;o[1]=r->type;o[2]=r->flags;p16(o+3,r->node_id);p16(o+5,r->sequence);p32(o+7,r->uptime_s);p16(o+11,r->vibration_rms_mg);p16(o+13,r->battery_mv);o[15]=0;return 0;}
int ss_decode(const uint8_t *i,unsigned n,ss_record_t *r){if(!i||!r||n!=SS_FRAME_SIZE||i[0]!=SS_VERSION||i[1]!=SS_TYPE_TELEMETRY||i[15]!=0)return -1;r->version=i[0];r->type=i[1];r->flags=i[2];r->node_id=g16(i+3);r->sequence=g16(i+5);r->uptime_s=g32(i+7);r->vibration_rms_mg=g16(i+11);r->battery_mv=g16(i+13);return 0;}

#include <stdint.h>
#include <stddef.h>

/* Stable little-endian binary sample frame: BG, version, kind, seq, signed value. */
#define BG_FRAME_SIZE 10u
int bg_encode(uint8_t kind, uint32_t seq, int32_t value, uint8_t out[BG_FRAME_SIZE]) {
    if (!out || (kind != 1u && kind != 2u)) return -1;
    out[0]='B'; out[1]='G'; out[2]=1; out[3]=kind;
    for (unsigned i=0;i<4;i++) out[4+i]=(uint8_t)(seq >> (8*i));
    for (unsigned i=0;i<2;i++) out[8+i]=(uint8_t)((uint32_t)value >> (8*i));
    return (int)BG_FRAME_SIZE;
}
int bg_decode(const uint8_t *in, size_t n, uint8_t *kind, uint32_t *seq, int16_t *value) {
    if (!in || !kind || !seq || !value || n != BG_FRAME_SIZE || in[0]!='B' || in[1]!='G' || in[2]!=1 || (in[3]!=1 && in[3]!=2)) return -1;
    *kind=in[3]; *seq=(uint32_t)in[4]|((uint32_t)in[5]<<8)|((uint32_t)in[6]<<16)|((uint32_t)in[7]<<24);
    *value=(int16_t)((uint16_t)in[8]|((uint16_t)in[9]<<8)); return 0;
}

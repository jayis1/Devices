#include <stdint.h>
/* BLE payload is coarse and non-identifying; pack_id is a site-randomized label, not a serial number. */
typedef struct { uint8_t pack_id[8]; uint8_t soc_bucket; int16_t temp_deci_c; } pack_status_t;
int pack_status_valid(const pack_status_t *p) { if(!p||p->soc_bucket>4||p->temp_deci_c < -200||p->temp_deci_c>800) return 0; uint8_t nonzero=0;for(unsigned i=0;i<8;i++)nonzero|=p->pack_id[i];return nonzero!=0; }

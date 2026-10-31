#include "../common/protocol.h"
/* Bench dock event producer; analog interface via ADS1115, no autonomous chemical handling. */
#define I2C_SDA 4
#define I2C_SCL 5
int dock_event(labsignal_message_t *m,uint32_t seq,int32_t event_milli){if(!m)return -1;m->kind=2;m->sequence=seq;m->value_milli=event_milli;return 0;}

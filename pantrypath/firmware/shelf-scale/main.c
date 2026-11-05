#include <stdint.h>
#include "../common/protocol.h"
#define NODE_ID 1
#define I2C_SDA 6
#define I2C_SCL 7
/* ESP-IDF board/driver integration remains TODO. Encode stable load delta, grams. */
int make_delta(uint8_t seq,uint32_t epoch,int16_t grams,pp_msg_t*m){if(!m||grams < -5000||grams>5000)return -1;m->version=PP_VERSION;m->node=NODE_ID;m->type=1;m->seq=seq;m->epoch_s=epoch;m->len=2;m->payload[0]=(uint8_t)grams;m->payload[1]=(uint8_t)(grams>>8);return 0;}

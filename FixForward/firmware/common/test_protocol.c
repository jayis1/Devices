#include "protocol.h"
#include <assert.h>
int main(void){ff_message m={1,1,7,1,250,1};assert(ff_valid(&m));m.version=2;assert(!ff_valid(&m));assert(ff_crc16((const uint8_t*)"123456789",9)==0x29b1);return 0;}

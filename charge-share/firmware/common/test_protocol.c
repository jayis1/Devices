#include "protocol.h"
#include <assert.h>
int main(void){uint8_t f[CHARGESHARE_FRAME_LEN];assert(charges_encode(f,sizeof f,3,9,99,5000,800)==20);assert(charges_validate(f,sizeof f));f[4]^=1;assert(!charges_validate(f,sizeof f));return 0;}

#include "protocol.h"
#include <assert.h>
int main(void){uint8_t b[AIRSHARE_FRAME_LEN];assert(airshare_encode(b,sizeof b,42,9,1234,800,4550)==20);assert(airshare_validate(b,sizeof b));b[5]^=1;assert(!airshare_validate(b,sizeof b));assert(!airshare_encode(b,2,1,1,1,1,1));return 0;}

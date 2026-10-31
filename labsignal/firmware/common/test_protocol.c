#include "protocol.h"
#include <assert.h>
int main(void){labsignal_message_t a={2,0x12345678,-4321},b={0};uint8_t f[LABSIGNAL_FRAME_SIZE];assert(labsignal_encode(&a,f)==0);assert(labsignal_decode(f,sizeof f,&b)==0);assert(a.kind==b.kind&&a.sequence==b.sequence&&a.value_milli==b.value_milli);assert(labsignal_decode(f,3,&b)<0);f[4]^=1;assert(labsignal_decode(f,sizeof f,&b)<0);return 0;}

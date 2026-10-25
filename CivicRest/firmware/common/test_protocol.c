#include <assert.h>
#include <string.h>
#include "protocol.h"
int main(void){
 const uint8_t check[]="123456789"; assert(cr_crc16(check,9)==0x29b1);
 uint8_t frame[CR_MAX_FRAME],type=0,len=0; uint16_t id=0,seq=0; size_t n=0; const uint8_t *payload=0;
 const uint8_t body[]={3,4}; assert(cr_encode(frame,sizeof frame,CR_TELEMETRY,42,9,body,2,&n)==0);
 assert(cr_decode(frame,n,&type,&id,&seq,&payload,&len)==0); assert(type==CR_TELEMETRY&&id==42&&seq==9&&len==2&&memcmp(payload,body,2)==0);
 frame[7]^=1; assert(cr_decode(frame,n,&type,&id,&seq,&payload,&len)==-2); return 0;
}

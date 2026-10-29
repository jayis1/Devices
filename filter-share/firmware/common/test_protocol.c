#include "protocol.h"
#include <assert.h>
int main(void) {
  uint8_t b[FS_FRAME_LEN], t; uint32_t id,s,u; int32_t a,c;
  assert(fs_encode(b,1,42,7,123,-11,99)==0);
  assert(fs_decode(b,sizeof b,&t,&id,&s,&u,&a,&c)==0);
  assert(t==1 && id==42 && s==7 && u==123 && a==-11 && c==99);
  assert(fs_decode(b,sizeof b-1,&t,&id,&s,&u,&a,&c)<0);
  b[10]^=1; assert(fs_decode(b,sizeof b,&t,&id,&s,&u,&a,&c)==-2);
  return 0;
}

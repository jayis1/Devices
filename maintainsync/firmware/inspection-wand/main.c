#include <stdint.h>
#include "../common/protocol.h"
extern int mlx90640_frame(int16_t *dst); extern int qr_asset(char *dst,unsigned n); extern void wifi_send(ms_frame_t*);
int main(void){int16_t px[768];char asset[16];uint32_t seq=0;for(;;){/* GPIO0 trigger is debounced by BSP */ if(!qr_asset(asset,sizeof asset))continue; mlx90640_frame(px); ms_frame_t f={.version=1,.type=MS_INSPECTION,.node_id=0x2001,.seq=++seq,.len=12}; for(int i=0;i<4;i++)((int16_t*)f.payload)[i]=px[i*192]; f.crc32c=ms_crc32c((uint8_t*)&f,offsetof(ms_frame_t,crc32c));wifi_send(&f);}}

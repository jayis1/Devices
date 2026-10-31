#include <stdint.h>
#include "../common/protocol.h"
/* Host-buildable frame exerciser; gateway hardware integrations are platform-specific. */
int main(void) {
    cr_message_t in={0}, out={0}; uint8_t frame[CR_FRAME_MAX];
    in.version=CR_VERSION; in.node_id=1; in.type=1; in.seq=1; in.timestamp_s=1;
    int n=cr_encode(&in,frame,sizeof frame);
    if (n<0 || cr_decode(frame,(uint16_t)n,&out)!=0) return 1;
    return out.node_id==in.node_id ? 0 : 2;
}

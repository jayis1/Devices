#include "../common/protocol.h"
/* Hub reference: validate frame then aggregate; platform transport/storage omitted. */
int main(void) {
    uint8_t frame[LL_FRAME_SIZE]={0}; ll_telemetry_t msg={0};
    /* TODO target: poll RS-485 nodes with timeout; validate duplicate sequence before cache. */
    (void)ll_decode(&msg,frame,sizeof frame);
    return 0;
}

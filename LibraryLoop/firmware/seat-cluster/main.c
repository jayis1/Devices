#include "../common/protocol.h"
/* Portable sensing loop reference; target GPIO/ADC/RS-485 HAL intentionally omitted. */
int main(void) {
    uint16_t sequence=0u, occupied=0u, valid=0u;
    for (;;) {
        /* Read four calibrated pads; on sensor fault clear valid bit, never report free. */
        uint8_t frame[LL_FRAME_SIZE];
        ll_telemetry_t sample={.kind=1u,.node_id=1u,.sequence=sequence++, .occupied_mask=occupied,.valid_mask=valid,.uptime_s=0u};
        if (ll_encode(frame,&sample)==0) { /* TODO target: send addressed Modbus frame, await bounded response. */ }
        break; /* Single-cycle compileable reference, not a deployable firmware image. */
    }
    return 0;
}

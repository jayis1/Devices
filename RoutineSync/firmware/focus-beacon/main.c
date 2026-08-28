#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static uint8_t classify_focus_state(uint16_t seat_exits, uint16_t noise_db_x10, uint16_t session_minutes) {
    if (session_minutes > 95u && seat_exits == 0u) {
        return RS_FOCUS_HYPERFOCUS;
    }
    if (seat_exits > 4u || noise_db_x10 > 620u) {
        return RS_FOCUS_DISTRACTED;
    }
    if (session_minutes > 50u) {
        return RS_FOCUS_TRANSITION;
    }
    return RS_FOCUS_FOCUSED;
}

int main(void) {
    rs_mesh_ctx_t mesh;
    rs_mesh_init(&mesh, RS_NODE_FOCUS_BEACON, 0xF0C05AA1u);

    rs_focus_state_t state;
    memset(&state, 0, sizeof(state));
    state.occupancy = 1;
    state.voc_index = 144u;
    state.light_lux = 380u;
    state.noise_db_x10 = 455u;
    state.seat_exit_count = 1u;
    state.session_minutes = 102u;
    state.focus_state = classify_focus_state(state.seat_exit_count, state.noise_db_x10, state.session_minutes);
    state.cue_level = (state.focus_state == RS_FOCUS_HYPERFOCUS) ? 2u : 1u;

    rs_frame_t frame;
    rs_build_frame(&frame,
                   mesh.node_id,
                   RS_NODE_HUB,
                   RS_MSG_FOCUS_STATE,
                   mesh.next_seq++,
                   mesh.session_nonce,
                   (const uint8_t *)&state,
                   (uint8_t)sizeof(state));

    printf("[focus] state=%u session=%umin voc=%u light=%ulx cue=%u crc=0x%04X\n",
           state.focus_state,
           state.session_minutes,
           state.voc_index,
           state.light_lux,
           state.cue_level,
           frame.crc);
    return 0;
}

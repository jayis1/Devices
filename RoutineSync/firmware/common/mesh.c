#include "mesh.h"
#include <string.h>

void rs_mesh_init(rs_mesh_ctx_t *ctx, uint16_t node_id, uint32_t nonce) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->node_id = node_id;
    ctx->session_nonce = nonce;
    ctx->next_seq = 1;
    ctx->joined = true;
}

void rs_mesh_make_heartbeat(rs_mesh_ctx_t *ctx, rs_frame_t *frame, uint16_t dst) {
    const uint8_t heartbeat[4] = {0x01, 0x00, 0x00, 0x00};
    rs_build_frame(frame, ctx->node_id, dst, RS_MSG_HEARTBEAT, ctx->next_seq++, ctx->session_nonce, heartbeat, sizeof(heartbeat));
}

bool rs_mesh_is_for_me(const rs_frame_t *frame, uint16_t node_id) {
    return frame->dst_id == node_id || frame->dst_id == RS_NODE_BROADCAST;
}

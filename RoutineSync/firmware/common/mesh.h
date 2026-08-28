#ifndef ROUTINESYNC_MESH_H
#define ROUTINESYNC_MESH_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

typedef struct {
    uint16_t node_id;
    uint32_t session_nonce;
    uint16_t next_seq;
    bool joined;
} rs_mesh_ctx_t;

void rs_mesh_init(rs_mesh_ctx_t *ctx, uint16_t node_id, uint32_t nonce);
void rs_mesh_make_heartbeat(rs_mesh_ctx_t *ctx, rs_frame_t *frame, uint16_t dst);
bool rs_mesh_is_for_me(const rs_frame_t *frame, uint16_t node_id);

#endif

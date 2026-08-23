#include "mesh.h"
#include <stdio.h>

static mesh_config_t g_config;

void mesh_init(const mesh_config_t *config) {
    if (config) {
        g_config = *config;
    }
}

bool mesh_send_frame(const psync_frame_t *frame) {
    (void)frame;
    return true;
}

bool mesh_receive_frame(psync_frame_t *frame) {
    (void)frame;
    return false;
}

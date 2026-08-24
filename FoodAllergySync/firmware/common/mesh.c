#include "mesh.h"
#include <string.h>

static fas_mesh_config_t g_config;

void fas_mesh_init(const fas_mesh_config_t *config) {
    if (config) {
        g_config = *config;
    } else {
        memset(&g_config, 0, sizeof(g_config));
    }
}

int fas_mesh_send(const fas_frame_t *frame) {
    return frame ? 0 : -1;
}

int fas_mesh_poll(fas_frame_t *frame) {
    (void)frame;
    return 0;
}

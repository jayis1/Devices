#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

typedef struct {
    cs_dock_state_t state;
    bool interlock_closed;
} dock_runtime_t;

static bool dock_ready(const dock_runtime_t *runtime) {
    return runtime->state.clean_tank_ml >= 900u &&
           runtime->state.dirty_tank_ml <= 150u &&
           runtime->state.detergent_ml >= 80u &&
           runtime->state.leak_detected == 0u &&
           runtime->interlock_closed;
}

int main(void) {
    dock_runtime_t runtime = {
        .state = {
            .clean_tank_ml = 1400u,
            .dirty_tank_ml = 90u,
            .detergent_ml = 220u,
            .pump_state = 1u,
            .leak_detected = 0u,
            .current_ma = 620u,
        },
        .interlock_closed = true,
    };

    printf("dock ready=%u clean=%u dirty=%u detergent=%u current=%u\n",
           dock_ready(&runtime) ? 1u : 0u,
           runtime.state.clean_tank_ml,
           runtime.state.dirty_tank_ml,
           runtime.state.detergent_ml,
           runtime.state.current_ma);
    return 0;
}

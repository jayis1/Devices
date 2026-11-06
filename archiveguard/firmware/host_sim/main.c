#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "../common/protocol.h"
int main(void) {
    const struct telemetry t = {1, 1, 7, 20.5f, 48.0f};
    if (!telemetry_valid(&t)) return 1;
    printf("schema=%u node=%u seq=%u temp=%.1f rh=%.1f\n", t.version, t.node_id, t.seq, t.temperature_c, t.rh_pct);
    return 0;
}

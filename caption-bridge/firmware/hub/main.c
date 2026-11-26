#include "../common/protocol.h"

/* Host model only: production BLE collection and provisioning are absent. */
int main(void) {
    return cb_seq_is_newer(2u, 1u) && !cb_seq_is_newer(1u, 2u) ? 0 : 1;
}

#include <stdbool.h>
#include "../common/protocol.h"
/* Crew acknowledgement never clears an incident without hub-side inspection workflow. */
cu_event_t crew_ack(uint32_t seq, uint32_t ms, bool confirmed) {
    return (cu_event_t){seq, ms, CU_ACK, confirmed ? 1u : 0u};
}

#include <stdint.h>
#include <stdbool.h>

/* Gateway host-side ingestion seam. BLE adapter and platform integration TODO. */
struct report { uint8_t version; uint8_t node_id[8]; uint32_t seq; uint32_t uptime_s; uint16_t voltage_centivolt; uint16_t current_ma; int16_t temp_c_centi; uint16_t flags; };
bool report_acceptable(const struct report *r) {
    if (r == 0 || r->version != 1u || r->seq == 0u) return false;
    if (r->voltage_centivolt > 30000u || r->current_ma > 30000u) return false;
    return true;
}
int main(void) { return 0; }

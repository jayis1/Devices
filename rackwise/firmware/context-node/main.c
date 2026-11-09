#include "../common/protocol.h"
/* Sensor driver and I2C/MQTT target integration remain platform-specific. */
int main(void) {
    rw_rack_report_t empty = {.seq = 0, .count = 1, .bay = {RW_UNKNOWN}};
    return empty.count == 1 ? 0 : 1;
}

#include "../common/protocol.h"
/* Board adapter: read INA219 bus voltage/current and ADS1115 pH/probe input; publish calibrated readings. */
#define I2C_SDA 4
#define I2C_SCL 5
int sensor_node_sample(labsignal_message_t *m, uint32_t seq, int32_t millivolts) { if(!m)return -1;m->kind=1;m->sequence=seq;m->value_milli=millivolts;return 0; }

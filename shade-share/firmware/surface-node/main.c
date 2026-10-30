#include "../common/protocol.h"
/* HAL adapter supplies ADC and radio APIs; report centi-degrees C. */
int surface_node_sample(int16_t temp_centi_c, uint16_t battery_mv, uint8_t *frame, uint32_t id, uint16_t seq) { return ss_encode(frame,id,seq,3,temp_centi_c,battery_mv); }

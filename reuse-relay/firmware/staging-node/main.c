#include "../common/protocol.h"
/* ESP-IDF integration pending: sample sensor values, encode, then send over bonded BLE GATT. */
int staging_sample_frame(uint32_t node,uint32_t seq,int32_t mg,uint8_t flags,uint8_t out[RR_FRAME_SIZE]) { rr_message_t m={RR_WEIGHT_SAMPLE,node,seq,mg,flags};return rr_encode(&m,out); }

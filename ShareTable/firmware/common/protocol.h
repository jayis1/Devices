#include <stdint.h>
#define ST_PROTOCOL_VERSION 1
#define ST_MAX_PACKET 512
struct st_reading { uint8_t version; uint32_t seq; float value; };

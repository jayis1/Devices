#include <stddef.h>
#include <stdint.h>
#define AIRSHARE_FRAME_LEN 20u
int airshare_encode(uint8_t *out, size_t cap, uint32_t node, uint32_t seq,
                    uint32_t uptime_ms, uint16_t co2_ppm, uint16_t rh_centi);
int airshare_validate(const uint8_t *frame, size_t len);

#include <stdint.h>
#include <stdbool.h>

/* Integration shell: board-specific I2S/GATT drivers intentionally required. */
extern bool audio_read_1s(int32_t *buffer, uint32_t capacity, uint32_t *count);
extern void ble_send_level(uint32_t seq, uint16_t db10);
extern uint16_t qh_db10_bucket(const int32_t *, uint32_t);

void app_main(void) {
    static int32_t samples[16000];
    uint32_t count = 0, seq = 0;
    for (;;) {
        if (audio_read_1s(samples, 16000, &count) && count <= 16000) {
            uint16_t estimate = qh_db10_bucket(samples, count);
            ble_send_level(++seq, estimate);
        }
        /* Erase the transient audio buffer; never persist or transmit PCM. */
        for (uint32_t i = 0; i < 16000; ++i) samples[i] = 0;
    }
}

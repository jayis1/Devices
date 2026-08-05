/* SeizureSync — SX1262 driver header (Sub-GHz radio) */
#ifndef SX1262_H
#define SX1262_H
#include <stdint.h>
#include <stddef.h>
void sx1262_init(int nss, int rst, int dio0, int busy,
                 int sck, int miso, int mosi);
void sx1262_set_frequency(uint32_t hz);
void sx1262_set_tx_power(int8_t dbm);
void sx1262_set_modem_params(uint32_t bw, uint8_t sf, uint8_t cr);
int  sx1262_send(const uint8_t *data, size_t len);
int  sx1262_receive(uint8_t *buf, size_t maxlen, uint32_t timeout_ms);
#endif
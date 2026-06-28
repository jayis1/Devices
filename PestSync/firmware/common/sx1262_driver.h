/*
 * SX1262 Sub-GHz LoRa Driver — Header
 * sx1262_driver.h
 */
#ifndef SX1262_DRIVER_H
#define SX1262_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    void (*spi_transfer)(uint8_t *tx, uint8_t *rx, size_t len);
    void (*cs_select)(void);
    void (*cs_deselect)(void);
    void (*reset)(bool asserted);   /* true = reset low */
    void (*wait_busy)(void);
    bool (*get_dio1)(void);
    uint32_t (*millis)(void);
} sx1262_hal_t;

typedef struct {
    sx1262_hal_t hal;
    uint8_t  sync_word;
    uint32_t freq_hz;
    uint32_t bw_hz;
    uint8_t  sf;
    uint8_t  cr;
    int8_t   tx_power_dbm;
    uint8_t  aes_key[16];
    bool     aes_set;
} sx1262_t;

int  sx1262_init(sx1262_t *radio);
void sx1262_set_key(sx1262_t *radio, const uint8_t *key);
int  sx1262_tx(sx1262_t *radio, const uint8_t *data, size_t len);
int  sx1262_rx(sx1262_t *radio, uint32_t timeout_ms, uint8_t *data, size_t max_len, int *rssi);
void sx1262_sleep(sx1262_t *radio);
void sx1262_set_tx_power(sx1262_t *radio, int8_t dbm);

#endif /* SX1262_DRIVER_H */
/*
 * StormSync — SX1262 Radio Driver (Header)
 * Semtech SX1262 Sub-GHz LoRa transceiver interface
 */
#ifndef STORMSYNC_SX1262_H
#define STORMSYNC_SX1262_H

#include <stdint.h>
#include <stddef.h>

/* Radio configuration */
typedef struct {
    uint32_t frequency;       /* Hz, e.g. 868000000 */
    uint32_t bandwidth;       /* Hz, e.g. 125000 */
    uint8_t  spreading_factor;/* 7-12 */
    uint8_t  coding_rate;     /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */
    uint8_t  preamble_len;    /* Symbols */
    int8_t   tx_power_dbm;    /* -9 to +22 */
    uint32_t rx_timeout_ms;   /* 0 = continuous */
} ss_radio_config_t;

/* Platform SPI interface (implemented per-node) */
typedef struct {
    void (*init)(void);
    void (*cs_select)(void);
    void (*cs_release)(void);
    uint8_t (*transfer)(uint8_t byte);
    void (*reset)(uint8_t assert);
    void (*delay_ms)(uint32_t ms);
    int  (*dio1_read)(void);
    void (*dio1_irq_enable)(int enable);
} ss_spi_interface_t;

/* Radio status */
typedef enum {
    SS_RADIO_IDLE = 0,
    SS_RADIO_RX,
    SS_RADIO_TX,
    SS_RADIO_SLEEP,
} ss_radio_state_t;

/* Initialize radio with given config */
int ss_radio_init(const ss_spi_interface_t *spi, const ss_radio_config_t *cfg);

/* Transmit data (blocking until TX complete) */
int ss_radio_tx(const uint8_t *data, uint8_t len);

/* Receive data (returns length, 0 on timeout, -1 on error) */
int ss_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms);

/* Get last received RSSI */
int8_t ss_radio_get_rssi(void);

/* Enter sleep mode */
void ss_radio_sleep(void);

/* Wake from sleep */
int ss_radio_wakeup(void);

/* Get current state */
ss_radio_state_t ss_radio_get_state(void);

#endif /* STORMSYNC_SX1262_H */
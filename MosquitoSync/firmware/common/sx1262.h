/*
 * MosquitoSync — SX1262 Radio Driver (Header)
 * Semtech SX1262 Sub-GHz LoRa transceiver interface
 */
#ifndef MOSQUITOSYNC_SX1262_H
#define MOSQUITOSYNC_SX1262_H

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
} ms_radio_config_t;

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
} ms_spi_interface_t;

/* Radio status */
typedef enum {
    MS_RADIO_IDLE = 0,
    MS_RADIO_RX,
    MS_RADIO_TX,
    MS_RADIO_SLEEP,
} ms_radio_state_t;

/* Initialize radio with given config */
int ms_radio_init(const ms_spi_interface_t *spi, const ms_radio_config_t *cfg);

/* Transmit data (blocking until TX complete) */
int ms_radio_tx(const uint8_t *data, uint8_t len);

/* Receive data (returns length, 0 on timeout, -1 on error) */
int ms_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms);

/* Get last received RSSI */
int8_t ms_radio_get_rssi(void);

/* Enter sleep mode */
void ms_radio_sleep(void);

/* Wake from sleep */
int ms_radio_wakeup(void);

/* Get current state */
ms_radio_state_t ms_radio_get_state(void);

#endif /* MOSQUITOSYNC_SX1262_H */
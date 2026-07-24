/*
 * EchoSync — SX1262 Sub-GHz Radio Driver (Header)
 */
#ifndef ECHOSYNC_SX1262_H
#define ECHOSYNC_SX1262_H

#include <stdint.h>
#include <stddef.h>

/* Radio configuration */
typedef struct {
    uint32_t frequency;
    uint32_t bandwidth;
    uint8_t  spreading_factor;
    uint8_t  coding_rate;
    uint8_t  preamble_len;
    int8_t   tx_power_dbm;
    uint32_t rx_timeout_ms;
} es_radio_config_t;

/* SPI interface (platform-abstracted) */
typedef struct {
    void (*init)(void);
    void (*cs_select)(void);
    void (*cs_release)(void);
    uint8_t (*transfer)(uint8_t byte);
    void (*reset)(uint8_t assert);
    void (*delay_ms)(uint32_t ms);
    int  (*dio1_read)(void);
    void (*dio1_irq_enable)(int enable);
} es_spi_interface_t;

/* Radio context */
typedef struct {
    const es_spi_interface_t *spi;
    es_radio_config_t config;
    int initialized;
} es_radio_ctx_t;

/* Initialize radio */
int es_radio_init(es_radio_ctx_t *ctx, const es_spi_interface_t *spi,
                  const es_radio_config_t *cfg);

/* Transmit data */
int es_radio_tx(const uint8_t *data, uint8_t len);

/* Receive data (returns length, 0 on timeout, -1 on error) */
int es_radio_rx(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms);

/* Get RSSI of last received packet */
int8_t es_radio_get_rssi(void);

/* Set TX power */
void es_radio_set_tx_power(int8_t dbm);

/* Sleep mode */
void es_radio_sleep(void);

/* Wake from sleep */
void es_radio_wakeup(void);

#endif /* ECHOSYNC_SX1262_H */
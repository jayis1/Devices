/*
 * LawnSync — SX1262 Sub-GHz Radio Driver (Header)
 * Supports SX1262 (soil/sprinkler/weather/scanner) and SX1276 (hub)
 * SPI-based control, LoRa modulation, TX/RX, CAD
 */
#ifndef LAWNSYNC_SX1262_H
#define LAWNSYNC_SX1262_H

#include <stdint.h>
#include <stddef.h>

/* Radio configuration */
#define LS_FREQ_868MHZ     868000000  /* Hz */
#define LS_FREQ_915MHZ     915000000
#define LS_BW_125KHZ       125000
#define LS_BW_250KHZ       250000
#define LS_SF_7            7
#define LS_SF_8            8
#define LS_SF_9            9
#define LS_SF_10           10
#define LS_SF_11           11
#define LS_CR_4_5          0x01   /* Coding rate 4/5 */
#define LS_PREAMBLE_LEN    8
#define LS_TX_POWER_22DBM  22    /* SX1262 max */
#define LS_TX_POWER_20DBM  20    /* SX1276 max */

/* Radio states */
typedef enum {
    LS_RADIO_IDLE = 0,
    LS_RADIO_TX,
    LS_RADIO_RX,
    LS_RADIO_CAD,
    LS_RADIO_SLEEP,
} ls_radio_state_t;

/* Configuration */
typedef struct {
    uint32_t frequency;       /* Hz */
    uint32_t bandwidth;        /* Hz */
    uint8_t  spreading_factor; /* 7-12 */
    uint8_t  coding_rate;      /* 0x01 = 4/5, etc. */
    uint8_t  preamble_len;     /* symbols */
    int8_t   tx_power_dbm;     /* dBm */
    uint16_t rx_timeout_ms;    /* 0 = continuous RX */
} ls_radio_config_t;

/* TX/RX result */
typedef struct {
    uint8_t  rssi;        /* dBm (signed, stored as uint8) */
    uint8_t  snr;         /* dB (signed) */
    uint8_t  data[256];
    uint8_t  data_len;
} ls_radio_packet_t;

/* Platform SPI interface (must be implemented by each node) */
typedef struct {
    void (*init)(void);
    void (*cs_select)(void);
    void (*cs_release)(void);
    uint8_t (*transfer)(uint8_t byte);
    void (*reset)(uint8_t assert);  /* assert = 1: reset low, 0: release */
    void (*delay_ms)(uint32_t ms);
    int  (*dio1_read)(void);        /* returns 1 if DIO1 is high */
    void (*dio1_irq_enable)(int enable);
} ls_spi_interface_t;

/* Initialize radio with config */
int ls_radio_init(const ls_spi_interface_t *spi, const ls_radio_config_t *cfg);

/* Send a packet (blocking, returns bytes sent or -1) */
int ls_radio_tx(const uint8_t *data, uint8_t len);

/* Receive a packet (blocking with timeout, returns bytes received or -1) */
int ls_radio_rx(ls_radio_packet_t *pkt, uint32_t timeout_ms);

/* Enter sleep (lowest power) */
int ls_radio_sleep(void);

/* Wake from sleep */
int ls_radio_wakeup(void);

/* Channel Activity Detection — returns 1 if channel is busy */
int ls_radio_cad(uint32_t timeout_ms);

/* Get current RSSI during RX */
int8_t ls_radio_get_rssi(void);

/* Set radio configuration (can change SF, BW, freq at runtime) */
int ls_radio_set_config(const ls_radio_config_t *cfg);

#endif /* LAWNSYNC_SX1262_H */
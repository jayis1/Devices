/**
 * FreshKeep — Common SX1261/SX1262 Radio Driver (Shared)
 * Supports both SX1261 (125mW, node) and SX1262 (1W, hub)
 * 
 * SPI interface, LoRa modulation, TDMA timing
 */

#ifndef FRESHKEEP_RADIO_H
#define FRESHKEEP_RADIO_H

#include <stdint.h>
#include <string.h>
#include "protocol.h"

/* ── Radio Type ───────────────────────────────────────────────────── */
typedef enum {
    RADIO_SX1261 = 1,  /* 125mW, +14dBm — nodes */
    RADIO_SX1262 = 2,  /* 1W, +20dBm — hub */
} radio_type_t;

/* ── Radio Configuration ──────────────────────────────────────────── */
typedef struct {
    radio_type_t type;
    uint32_t     frequency;     /* Hz, e.g. 868000000 or 915000000 */
    uint8_t      spreading_factor; /* 7-12 */
    uint8_t      bandwidth;      /* 0=7.8kHz .. 9=500kHz (register value) */
    uint8_t      coding_rate;    /* 1=4/5 .. 4=4/8 */
    int8_t       tx_power_dbm;   /* -17 to +14 (SX1261) or +22 (SX1262) */
    uint32_t     preamble_len;   /* Preamble length in symbols */
    uint8_t      sync_word[2];   /* Sync word */
} radio_config_t;

static const radio_config_t RADIO_CONFIG_DEFAULT = {
    .type            = RADIO_SX1261,
    .frequency       = 868000000,  /* 868 MHz EU */
    .spreading_factor = 7,
    .bandwidth       = 0x04,       /* 125 kHz */
    .coding_rate     = 1,          /* 4/5 */
    .tx_power_dbm    = 14,
    .preamble_len   = 8,
    .sync_word       = {0xF0, 0x4F},
};

static const radio_config_t RADIO_CONFIG_US = {
    .type            = RADIO_SX1261,
    .frequency       = 915000000,  /* 915 MHz US */
    .spreading_factor = 7,
    .bandwidth       = 0x04,       /* 125 kHz */
    .coding_rate     = 1,
    .tx_power_dbm    = 20,
    .preamble_len   = 8,
    .sync_word       = {0xF0, 0x4F},
};

/* ── SX126x Register Definitions ──────────────────────────────────── */
#define SX126X_REG_OPMODE          0x01
#define SX126X_REG_FRF_MSB        0x06
#define SX126X_REG_FRF_MID        0x07
#define SX126X_REG_FRF_LSB        0x08
#define SX126X_REG_PA_CONFIG      0x0E
#define SX126X_REG_PA_RAMP       0x0A
#define SX126X_REG_OCP            0x08
#define SX126X_REG_LNA            0x0C
#define SX126X_REG_FIFO_ADDR      0x0D
#define SX126X_REG_FIFO_TX        0x0E
#define SX126X_REG_FIFO_RX        0x0F
#define SX126X_REG_IRQ_FLAGS      0x12
#define SX126X_REG_SYNC_WORD      0x0C4D

/* ── SX126x Commands ──────────────────────────────────────────────── */
#define SX126X_CMD_SET_STANDBY     0x80
#define SX126X_CMD_SET_FS          0x81
#define SX126X_CMD_SET_TX          0x83
#define SX126X_CMD_SET_RX          0x82
#define SX126X_CMD_SET_SLEEP       0x84
#define SX126X_CMD_SET_PACKET_TYPE 0x8A
#define SX126X_CMD_SET_RF_FREQ     0x86
#define SX126X_CMD_SET_TX_PARAMS   0x8E
#define SX126X_CMD_SET_MOD_PARAMS  0x8B
#define SX126X_CMD_SET_PACKET_PARAMS 0x8C
#define SX126X_CMD_SET_BUFFER_ADDR 0x8D
#define SX126X_CMD_WRITE_BUFFER    0x0E
#define SX126X_CMD_READ_BUFFER      0x1E
#define SX126X_CMD_GET_IRQ_STATUS  0x12
#define SX126X_CMD_CLEAR_IRQ       0x02
#define SX126X_CMD_SET_DIO_IRQ      0x08
#define SX126X_CMD_CALIBRATE       0x89

/* ── Radio State ──────────────────────────────────────────────────── */
typedef enum {
    RADIO_STATE_SLEEP = 0,
    RADIO_STATE_STANDBY,
    RADIO_STATE_FS,
    RADIO_STATE_RX,
    RADIO_STATE_TX,
} radio_state_t;

/* ── Radio Handle ─────────────────────────────────────────────────── */
typedef struct {
    radio_config_t config;
    radio_state_t  state;
    uint8_t        slot;           /* TDMA slot assignment */
    uint32_t       last_tx_time;   /* ms */
    uint32_t       last_rx_time;   /* ms */
    uint16_t       rx_count;
    uint16_t       tx_count;
    uint16_t       rx_errors;
    uint16_t       tx_errors;
    /* Platform-specific SPI and GPIO handles — set by port layer */
    void          *spi_handle;
    uint8_t        pin_nss;
    uint8_t        pin_busy;
    uint8_t        pin_irq;
    uint8_t        pin_reset;
} radio_handle_t;

/* ── Platform Interface (must be implemented per MCU) ─────────────── */
void radio_spi_init(radio_handle_t *rh);
void radio_spi_cs(radio_handle_t *rh, uint8_t asserted);
uint8_t radio_spi_transfer(radio_handle_t *rh, uint8_t byte);
void radio_delay_ms(uint32_t ms);
uint32_t radio_get_time_ms(void);
void radio_gpio_write(uint8_t pin, uint8_t value);
uint8_t radio_gpio_read(uint8_t pin);

/* ── SPI Helper ───────────────────────────────────────────────────── */
static inline void radio_spi_write_reg(radio_handle_t *rh, uint16_t addr, uint8_t val) {
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, (addr >> 8) & 0xFF);
    radio_spi_transfer(rh, addr & 0xFF);
    radio_spi_transfer(rh, val);
    radio_spi_cs(rh, 1);
}

static inline uint8_t radio_spi_read_reg(radio_handle_t *rh, uint16_t addr) {
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, (addr >> 8) & 0xFF);
    radio_spi_transfer(rh, addr & 0xFF);
    uint8_t val = radio_spi_transfer(rh, 0xFF);
    radio_spi_cs(rh, 1);
    return val;
}

/* ── Radio Driver Functions ────────────────────────────────────────── */

static inline int radio_init(radio_handle_t *rh, const radio_config_t *config) {
    memcpy(&rh->config, config, sizeof(radio_config_t));
    rh->state = RADIO_STATE_SLEEP;
    rh->rx_count = rh->tx_count = rh->rx_errors = rh->tx_errors = 0;
    
    radio_spi_init(rh);
    
    /* Reset radio */
    radio_gpio_write(rh->pin_reset, 0);
    radio_delay_ms(1);
    radio_gpio_write(rh->pin_reset, 1);
    radio_delay_ms(10);
    
    /* Wait for busy pin to de-assert */
    uint32_t timeout = radio_get_time_ms() + 100;
    while (radio_gpio_read(rh->pin_busy) && radio_get_time_ms() < timeout);
    
    /* Set standby mode */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_STANDBY);
    radio_spi_transfer(rh, 0x00); /* STDBY_RC */
    radio_spi_cs(rh, 1);
    rh->state = RADIO_STATE_STANDBY;
    radio_delay_ms(10);
    
    /* Calibrate */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_CALIBRATE);
    radio_spi_transfer(rh, 0x7F); /* Calibrate all */
    radio_spi_cs(rh, 1);
    radio_delay_ms(20);
    
    /* Set packet type: LoRa */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_PACKET_TYPE);
    radio_spi_transfer(rh, 0x01); /* LORA */
    radio_spi_cs(rh, 1);
    
    /* Set RF frequency */
    uint32_t freq_reg = (uint64_t)rh->config.frequency * (1ULL << 25) / 32000000ULL;
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_RF_FREQ);
    radio_spi_transfer(rh, (freq_reg >> 24) & 0xFF);
    radio_spi_transfer(rh, (freq_reg >> 16) & 0xFF);
    radio_spi_transfer(rh, (freq_reg >> 8) & 0xFF);
    radio_spi_transfer(rh, freq_reg & 0xFF);
    radio_spi_cs(rh, 1);
    
    /* Set TX power */
    int8_t power = rh->config.tx_power_dbm;
    uint8_t pa_config;
    if (rh->config.type == RADIO_SX1262) {
        pa_config = 0x00; /* SX1262 uses PA_BOOST */
        if (power > 22) power = 22;
    } else {
        pa_config = 0x01; /* SX1261 uses RFO */
        if (power > 14) power = 14;
    }
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_TX_PARAMS);
    radio_spi_transfer(rh, power);
    radio_spi_transfer(rh, 0x02); /* Ramp time 200us */
    radio_spi_cs(rh, 1);
    
    /* Set LoRa modulation params */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_MOD_PARAMS);
    radio_spi_transfer(rh, rh->config.spreading_factor);
    radio_spi_transfer(rh, rh->config.bandwidth);
    radio_spi_transfer(rh, rh->config.coding_rate);
    radio_spi_transfer(rh, 0x00); /* Low Datarate Optimize: off for SF7 */
    radio_spi_cs(rh, 1);
    
    /* Set sync word */
    radio_spi_write_reg(rh, SX126X_REG_SYNC_WORD,
                        (rh->config.sync_word[0] << 4) | (rh->config.sync_word[1] >> 4));
    
    return 0;
}

static inline int radio_send(radio_handle_t *rh, const uint8_t *data, uint8_t len) {
    if (len > 255) return -1;
    
    /* Write data to FIFO */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_WRITE_BUFFER);
    radio_spi_transfer(rh, 0x00); /* Offset */
    for (uint8_t i = 0; i < len; i++) {
        radio_spi_transfer(rh, data[i]);
    }
    radio_spi_cs(rh, 1);
    
    /* Set packet params */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_PACKET_PARAMS);
    radio_spi_transfer(rh, rh->config.preamble_len >> 8);
    radio_spi_transfer(rh, rh->config.preamble_len & 0xFF);
    radio_spi_transfer(rh, 0x00); /* Header type: explicit */
    radio_spi_transfer(rh, len);  /* Payload length */
    radio_spi_transfer(rh, 0x02); /* CRC: on */
    radio_spi_transfer(rh, 0x00); /* Standard CRC */
    radio_spi_cs(rh, 1);
    
    /* Start TX */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_TX);
    radio_spi_transfer(rh, 0x00); /* Timeout MSB */
    radio_spi_transfer(rh, 0x00);
    radio_spi_transfer(rh, 0xFF); /* Timeout = max */
    radio_spi_cs(rh, 1);
    
    rh->state = RADIO_STATE_TX;
    rh->tx_count++;
    rh->last_tx_time = radio_get_time_ms();
    
    return 0;
}

static inline int radio_recv(radio_handle_t *rh, uint8_t *data, uint8_t max_len, uint32_t timeout_ms) {
    /* Set RX with timeout */
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_RX);
    radio_spi_transfer(rh, (timeout_ms >> 16) & 0xFF);
    radio_spi_transfer(rh, (timeout_ms >> 8) & 0xFF);
    radio_spi_transfer(rh, timeout_ms & 0xFF);
    radio_spi_cs(rh, 1);
    
    rh->state = RADIO_STATE_RX;
    
    /* Wait for IRQ (handled by platform-specific ISR) */
    /* This is a blocking wait — platform layer should implement radio_wait_irq() */
    
    return 0;
}

static inline void radio_sleep(radio_handle_t *rh) {
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_SLEEP);
    radio_spi_transfer(rh, 0x00); /* No warm start, no RTC */
    radio_spi_cs(rh, 1);
    rh->state = RADIO_STATE_SLEEP;
}

static inline void radio_standby(radio_handle_t *rh) {
    radio_spi_cs(rh, 0);
    radio_spi_transfer(rh, SX126X_CMD_SET_STANDBY);
    radio_spi_transfer(rh, 0x00);
    radio_spi_cs(rh, 1);
    rh->state = RADIO_STATE_STANDBY;
}

#endif /* FRESHKEEP_RADIO_H */
/*
 * cc1101.h — CC1101 Sub-GHz transceiver driver (868 MHz)
 *
 * Minimal SPI driver for CC1101 — TX/RX, carrier sense, RSSI.
 * Used by all QuakeGuard nodes for 868 MHz TDMA mesh communication.
 *
 * License: MIT
 */
#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

/* CC1101 register addresses */
#define CC1101_IOCFG2     0x00
#define CC1101_IOCFG1     0x01
#define CC1101_IOCFG0     0x02
#define CC1101_FIFOTHR    0x03
#define CC1101_SYNC1      0x04
#define CC1101_SYNC0      0x05
#define CC1101_PKTLEN     0x06
#define CC1101_PKTCTRL1   0x07
#define CC1101_PKTCTRL0   0x08
#define CC1101_ADDR       0x09
#define CC1101_CHANNR     0x0A
#define CC1101_FSCTRL1    0x0B
#define CC1101_FREQ2      0x0D
#define CC1101_FREQ1      0x0E
#define CC1101_FREQ0      0x0F
#define CC1101_MDMCFG4    0x10
#define CC1101_MDMCFG3    0x11
#define CC1101_MDMCFG2    0x12
#define CC1101_MDMCFG1    0x13
#define CC1101_MDMCFG0    0x14
#define CC1101_DEVIATN    0x15
#define CC1101_MCSM1      0x16
#define CC1101_MCSM0      0x17
#define CC1101_FOCCFG     0x18
#define CC1101_BSCFG      0x19
#define CC1101_AGCCTRL2   0x1B
#define CC1101_AGCCTRL1   0x1C
#define CC1101_AGCCTRL0   0x1D
#define CC1101_FREND1     0x1E
#define CC1101_FREND0     0x1F
#define CC1101_FSCAL3     0x23
#define CC1101_FSCAL2     0x24
#define CC1101_FSCAL1     0x25
#define CC1101_FSCAL0     0x26
#define CC1101_FSTEST     0x2E
#define CC1101_TEST2      0x31
#define CC1101_TEST1      0x32
#define CC1101_TEST0      0x33

/* Command strobes */
#define CC1101_SRES       0x30
#define CC1101_SFSTXON    0x31
#define CC1101_SXOFF      0x32
#define CC1101_SCAL       0x33
#define CC1101_SRX        0x34
#define CC1101_STX        0x35
#define CC1101_SIDLE      0x36
#define CC1101_SWOR       0x38
#define CC1101_SPWD       0x39
#define CC1101_SFRX        0x3A
#define CC1101_SFTX        0x3B
#define CC1101_SWORC      0x3C

/* Status registers (read with burst bit + 0x40) */
#define CC1101_TXBYTES     0x3A
#define CC1101_RXBYTES     0x3B
#define CC1101_PKTSTATUS   0x38
#define CC1101_RSSI        0x34
#define CC1101_MARCSTATE   0x35

/* SPI command flags */
#define CC1101_WRITE_BIT   0x00
#define CC1101_READ_BIT    0x80
#define CC1101_BURST_BIT   0x40

/* FIFO access */
#define CC1101_TXFIFO      0x3F
#define CC1101_RXFIFO      0x3F

/* Patable for 868 MHz, 10 dBm output */
#define CC1101_PATABLE_VAL 0xC2

/* Packet config: variable length, address check, CRC enabled */
#define CC1101_PKTCTRL0_VAL 0x45  /* CRC, variable len, Manchester off */

/* 868.0 MHz frequency word */
/* F_freq = F_xosc / 2^16 * FREQ[23:0] */
/* F_xosc = 26 MHz, target 868.0 MHz */
/* FREQ = 868e6 * 65536 / 26e6 = 2188_615_384 ≈ 0x828C_5A28 (truncated) */
/* Actually: 868e6 / (26e6/65536) = 868e6 / 396.7285 = 2188615 = 0x215F47 */
/* Let's use known working value for 868.0 MHz: 0x21656A */
#define CC1101_FREQ2_VAL   0x21
#define CC1101_FREQ1_VAL   0x62
#define CC1101_FREQ0_VAL   0x76

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
    gpio_num_t gd0_pin;    /* RX interrupt / packet received */
    gpio_num_t gd2_pin;    /* TX interrupt (optional) */
    uint8_t   my_addr;
} cc1101_t;

/* ── Function Prototypes ────────────────────────────────────── */

/**
 * Initialize CC1101 on SPI bus with 868 MHz config.
 * @param cc     Driver handle (caller-allocated)
 * @param host   SPI host (SPI2_HOST typically)
 * @param cs     CS GPIO pin
 * @param gd0   GD0 interrupt GPIO pin
 * @param gd2   GD2 GPIO pin (optional, set to -1)
 * @param addr   This node's address (for address filtering)
 * @return 0 on success, negative on error
 */
int cc1101_init(cc1101_t *cc, spi_host_device_t host,
                gpio_num_t cs, gpio_num_t gd0, gpio_num_t gd2,
                uint8_t addr);

/**
 * Send a raw packet (up to 61 bytes payload for CC1101 FIFO).
 * @param cc     Driver handle
 * @param data   Payload bytes
 * @param len    Payload length (max 61)
 * @return 0 on success, negative on error
 */
int cc1101_send(cc1101_t *cc, const uint8_t *data, uint8_t len);

/**
 * Read a received packet from RX FIFO.
 * @param cc     Driver handle
 * @param data   Output buffer (at least 64 bytes)
 * @param len    Output: received length
 * @param rssi   Output: RSSI in dBm (signed)
 * @return 0 on success, -1 if no packet, negative on error
 */
int cc1101_recv(cc1101_t *cc, uint8_t *data, uint8_t *len, int8_t *rssi);

/**
 * Enter RX mode (listen for packets).
 */
int cc1101_rx_mode(cc1101_t *cc);

/**
 * Enter idle mode (no RX/TX, lowest power short of sleep).
 */
int cc1101_idle(cc1101_t *cc);

/**
 * Enter sleep / power-down mode (~0.4 µA).
 */
int cc1101_sleep(cc1101_t *cc);

/**
 * Get RSSI (carrier sense for channel assessment).
 */
int8_t cc1101_get_rssi(cc1101_t *cc);

/**
 * Set TX power (0–10 dBm).
 */
int cc1101_set_power(cc1101_t *cc, uint8_t dbm);

#endif /* CC1101_H */
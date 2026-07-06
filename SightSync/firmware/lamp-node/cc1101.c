/**
 * SightSync Smart Lamp Node — CC1101 Sub-GHz Radio (868 MHz)
 *
 * Receives lamp commands from the hub. Same configuration as hub/desk.
 * License: MIT
 */

#include "cc1101.h"
#include "../common/crc8.h"
#include <Arduino.h>
#include <SPI.h>

static SPIClass *s_spi = NULL;
static uint8_t s_cs, s_gdo0;
static lamp_rx_cb_t s_rx_cb = NULL;
static bool s_initialized = false;

/* CC1101 register writes (simplified — see hub/subghz_radio.c for full config) */

static void cc1101_write_reg(uint8_t addr, uint8_t val)
{
    digitalWrite(s_cs, LOW);
    s_spi->transfer(addr & 0x7F);  /* write bit = 0 */
    s_spi->transfer(val);
    digitalWrite(s_cs, HIGH);
}

static uint8_t cc1101_read_reg(uint8_t addr)
{
    digitalWrite(s_cs, LOW);
    s_spi->transfer(addr | 0x80);  /* read bit = 1 */
    uint8_t val = s_spi->transfer(0x00);
    digitalWrite(s_cs, HIGH);
    return val;
}

static void cc1101_strobe(uint8_t cmd)
{
    digitalWrite(s_cs, LOW);
    s_spi->transfer(cmd);
    digitalWrite(s_cs, HIGH);
}

/* ── Init ─────────────────────────────────────────────────────────── */

void cc1101_init(uint8_t cs, uint8_t sck, uint8_t miso, uint8_t mosi,
                 uint8_t gdo0, lamp_rx_cb_t rx_cb)
{
    s_cs = cs;
    s_gdo0 = gdo0;
    s_rx_cb = rx_cb;

    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    pinMode(gdo0, INPUT);

    /* RP2040 SPI1 */
    s_spi = new SPIClass(spi1, sck, miso, mosi);
    s_spi->begin();
    s_spi->setFrequency(5000000);

    /* Reset CC1101 */
    cc1101_strobe(0x30);  /* SRES */
    delay(100);

    /* Frequency: 868.0 MHz */
    cc1101_write_reg(0x0D, 0x21);  /* FREQ2 */
    cc1101_write_reg(0x0E, 0x65);  /* FREQ1 */
    cc1101_write_reg(0x0F, 0x6B);  /* FREQ0 */

    /* Modem: GFSK, 38.4 kbaud */
    cc1101_write_reg(0x10, 0xCA);  /* MDMCFG4 */
    cc1101_write_reg(0x11, 0x83);  /* MDMCFG3 */
    cc1101_write_reg(0x12, 0x13);  /* MDMCFG2 */
    cc1101_write_reg(0x13, 0x22);  /* MDMCFG1 */
    cc1101_write_reg(0x14, 0xF8);  /* MDMCFG0 */
    cc1101_write_reg(0x15, 0x35);  /* DEVIATN */

    /* Packet: variable length, CRC */
    cc1101_write_reg(0x08, 0x05);  /* PKTCTRL0 */
    cc1101_write_reg(0x06, SS_MAX_PACKET_LEN);  /* PKTLEN */

    /* IO config */
    cc1101_write_reg(0x02, 0x06);  /* IOCFG0 */

    /* Enter RX mode */
    cc1101_strobe(0x34);  /* SRX */

    s_initialized = true;
    Serial.println("Lamp Node CC1101 initialized (868 MHz)");
}

/* ── Send ──────────────────────────────────────────────────────────── */

void cc1101_send(uint16_t dest_id, const uint8_t *data, uint8_t len)
{
    if (!s_initialized) return;
    (void)dest_id;

    cc1101_strobe(0x36);  /* SIDLE */
    cc1101_strobe(0x3B);  /* SFTX */

    /* Write to TX FIFO */
    digitalWrite(s_cs, LOW);
    s_spi->transfer(0x3F);  /* TXFIFO write */
    s_spi->transfer(len);
    for (uint8_t i = 0; i < len; i++) {
        s_spi->transfer(data[i]);
    }
    digitalWrite(s_cs, HIGH);

    cc1101_strobe(0x35);  /* STX */
    delay(10);
    cc1101_strobe(0x34);  /* SRX (return to RX) */
}

/* ── Poll for received packets (call from loop) ────────────────────── */

bool cc1101_is_ready(void)
{
    return s_initialized;
}

/* Note: GDO0 interrupt + RX FIFO read should be implemented in a
 * dedicated task. For this reference, the main loop calls cc1101_poll()
 * which checks for packet received. This is a simplified stub.
 */
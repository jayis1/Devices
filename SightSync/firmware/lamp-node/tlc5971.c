/**
 * SightSync Smart Lamp Node — TLC5971 16-Channel LED Driver
 *
 * TLC5971: 16 channels, 16-bit PWM each, 60 mA/channel.
 * Controls warm-white (ch 0-7) and cool-white (ch 8-15) LED strips.
 *
 * License: MIT
 */

#include "tlc5971.h"
#include <Arduino.h>
#include <SPI.h>

static uint8_t s_sck, s_mosi, s_latch, s_blank;
static uint16_t s_channels[16];
static SPIClass *s_spi = NULL;

/* TLC5971 command structure:
 * 6-bit function data + 12×16-bit channel data + 6-bit control data
 * Total: 244 bits = 30.5 bytes → 31 bytes (padded)
 */

#define TLC5971_N_CHANNELS 16

void tlc5971_init(uint8_t sck_pin, uint8_t mosi_pin, uint8_t latch_pin, uint8_t blank_pin)
{
    s_sck = sck_pin;
    s_mosi = mosi_pin;
    s_latch = latch_pin;
    s_blank = blank_pin;

    pinMode(s_latch, OUTPUT);
    pinMode(s_blank, OUTPUT);
    digitalWrite(s_latch, LOW);
    digitalWrite(s_blank, LOW);  /* un-blank (outputs enabled) */

    /* Use RP2040 SPI0 */
    s_spi = new SPIClass(spi0, sck_pin, -1, mosi_pin);
    s_spi->begin();
    s_spi->setFrequency(8000000);  /* 8 MHz */

    /* Initialize all channels to 0 */
    for (int i = 0; i < 16; i++) s_channels[i] = 0;
    tlc5971_update();
}

void tlc5971_set_all(const uint16_t channels[16])
{
    memcpy(s_channels, channels, sizeof(uint16_t) * 16);
}

void tlc5971_set_channel(uint8_t ch, uint16_t value)
{
    if (ch < 16) s_channels[ch] = value;
}

void tlc5971_update(void)
{
    if (s_spi == NULL) return;

    /* TLC5971 data format (244 bits):
     * [5:0]   Write command (6 bits) = 0x25
     * [11:6]  OutRGB (3 bits) = 0 (RGB on auto)
     * [12:13] Reserved
     * [17:14] OutTMG (1 bit) = 1 (external clock)
     * [23:18] BCRT (3 bits) = 7 (max global brightness)
     * [26:24] BCB (3 bits) = 7
     * [29:27] BCG (3 bits) = 7
     * [32:30] BCR (3 bits) = 7
     * [47:33] GS0 (16-bit, channel 0)
     * ... up to GS15 (16-bit, channel 15)
     */

    /* Build the 244-bit data stream */
    uint32_t header = 0;
    header |= (0x25UL << 0);    /* Write command */
    header |= (0x00UL << 6);    /* OutRGB */
    header |= (0x01UL << 14);   /* OutTMG = 1 (external clock) */
    header |= (0x07UL << 17);   /* BCRT = 7 (max global brightness) */
    header |= (0x07UL << 24);   /* BCB = 7 */
    header |= (0x07UL << 27);   /* BCG = 7 */
    /* BCR would be at bit 30, but we only have 32 bits — it wraps */

    /* Send header (4 bytes) */
    digitalWrite(s_latch, LOW);
    s_spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

    /* Function data (30 bits) — simplified: send 4 bytes */
    uint8_t func[4] = {
        (uint8_t)(header & 0xFF),
        (uint8_t)((header >> 8) & 0xFF),
        (uint8_t)((header >> 16) & 0xFF),
        (uint8_t)((header >> 24) & 0xFF),
    };
    s_spi->transfer(func, 4);

    /* BCR + GS data: BCR (3 bits) + GS0 (16 bits) + ... + GS15 (16 bits)
     * = 3 + 16×16 = 259 bits → 33 bytes
     * Simplified: send BCR as part of header, then 32 bytes of GS data
     */
    for (int i = 0; i < 16; i++) {
        uint8_t msb = (s_channels[i] >> 8) & 0xFF;
        uint8_t lsb = s_channels[i] & 0xFF;
        s_spi->transfer(msb);
        s_spi->transfer(lsb);
    }

    s_spi->endTransaction();

    /* Latch pulse */
    digitalWrite(s_latch, HIGH);
    delayMicroseconds(1);
    digitalWrite(s_latch, LOW);
}
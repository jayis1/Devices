/**
 * DriveSync CAN Driver — MCP2515
 *
 * SPI interface to MCP2515 CAN controller.
 * OBD-II standard baud rate: 500 kbps.
 *
 * License: MIT
 */

#include "can_driver.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string.h>

/* MCP2515 Registers */
#define MCP2515_RESET         0xC0
#define MCP2515_READ          0x03
#define MCP2515_WRITE         0x02
#define MCP2515_RTS           0x80
#define MCP2515_READ_STATUS   0xA0
#define MCP2515_BIT_MODIFY    0x05

#define MCP2515_CANSTAT       0x0E
#define MCP2515_CANCTRL       0x0F
#define MCP2515_CNF1          0x2A
#define MCP2515_CNF2          0x29
#define MCP2515_CNF3          0x28
#define MCP2515_RXB0CTRL      0x60
#define MCP2515_RXB1CTRL      0x70
#define MCP2515_TXB0CTRL      0x30
#define MCP2515_TXB0SIDH      0x31
#define MCP2515_TXB0DLC       0x35
#define MCP2515_TXB0D0        0x36
#define MCP2515_RXB0SIDH      0x61
#define MCP2515_RXB0DLC       0x65
#define MCP2515_RXB0D0        0x66
#define MCP2515_CANINTF       0x2C

#define CS_PIN  2
#define SPI_PORT spi0

/* ── SPI Helpers ──────────────────────────────────────────────────── */

static void cs_select(void)
{
    gpio_put(CS_PIN, 0);
}

static void cs_deselect(void)
{
    gpio_put(CS_PIN, 1);
}

static uint8_t spi_transfer(uint8_t data)
{
    uint8_t rx;
    spi_write_read_blocking(SPI_PORT, &data, &rx, 1);
    return rx;
}

static void mcp_write(uint8_t reg, uint8_t val)
{
    cs_select();
    spi_transfer(MCP2515_WRITE);
    spi_transfer(reg);
    spi_transfer(val);
    cs_deselect();
}

static uint8_t mcp_read(uint8_t reg)
{
    cs_select();
    spi_transfer(MCP2515_READ);
    spi_transfer(reg);
    uint8_t val = spi_transfer(0x00);
    cs_deselect();
    return val;
}

static void mcp_bit_modify(uint8_t reg, uint8_t mask, uint8_t val)
{
    cs_select();
    spi_transfer(MCP2515_BIT_MODIFY);
    spi_transfer(reg);
    spi_transfer(mask);
    spi_transfer(val);
    cs_deselect();
}

/* ── Init ──────────────────────────────────────────────────────────── */

void can_init(void)
{
    /* Initialize SPI */
    spi_init(SPI_PORT, 10000000);  /* 10 MHz */
    gpio_set_function(3, GPIO_FUNC_SPI);  /* SCK */
    gpio_set_function(4, GPIO_FUNC_SPI);  /* MOSI (TX) */
    gpio_set_function(5, GPIO_FUNC_SPI);  /* MISO (RX) */

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    cs_deselect();

    /* MCP2515 Reset */
    cs_select();
    spi_transfer(MCP2515_RESET);
    cs_deselect();
    sleep_ms(10);

    /* Set CONFIG mode (REQOP = 100) */
    mcp_write(MCP2515_CANCTRL, 0x80);
    sleep_ms(2);

    /* Configure bit timing for 500 kbps (from 16 MHz crystal) */
    /* CNF1: SJW=1, BRP=7 → 16 MHz / (2*(7+1)) = 1 MHz TQ clock */
    mcp_write(MCP2515_CNF1, 0x07);
    /* CNF2: BTLMODE=1, SAM=0, PHSEG1=6 → 7 TQ */
    mcp_write(MCP2515_CNF2, 0x8E);
    /* CNF3: PHSEG2=4 → 5 TQ → 7+8+5=20 TQ → 500 kbps */
    mcp_write(MCP2515_CNF3, 0x04);

    /* Configure RXB0: receive all, rollover enabled */
    mcp_write(MCP2515_RXB0CTRL, 0x64);
    mcp_write(MCP2515_RXB1CTRL, 0x60);

    /* Clear interrupts */
    mcp_write(MCP2515_CANINTF, 0x00);

    /* Set NORMAL mode (REQOP = 000) */
    mcp_write(MCP2515_CANCTRL, 0x00);
    sleep_ms(2);
}

/* ── Send/Receive ──────────────────────────────────────────────────── */

bool can_send(const can_frame_t *frame)
{
    if (frame == NULL) return false;
    if (frame->dlc > 8) return false;

    /* Wait until TXB0 is ready */
    uint32_t timeout = 100000;
    while ((mcp_read(MCP2515_TXB0CTRL) & 0x08) && --timeout) {
        tight_loop_contents();
    }
    if (timeout == 0) return false;

    /* Set CAN ID (standard 11-bit) */
    mcp_write(MCP2515_TXB0SIDH, (uint8_t)(frame->id >> 3));
    mcp_write(0x32, (uint8_t)(frame->id << 5));  /* TXB0SIDL */

    /* Set DLC */
    mcp_write(MCP2515_TXB0DLC, frame->dlc);

    /* Write data bytes */
    for (uint8_t i = 0; i < frame->dlc; i++) {
        mcp_write(MCP2515_TXB0D0 + i, frame->data[i]);
    }

    /* Request to send TXB0 */
    cs_select();
    spi_transfer(MCP2515_RTS | 0x01);
    cs_deselect();

    return true;
}

bool can_receive(can_frame_t *frame)
{
    if (frame == NULL) return false;

    /* Check if RXB0 has data (CANINTF RX0IF) */
    uint8_t status = mcp_read(MCP2515_CANINTF);
    if (!(status & 0x01)) return false;

    /* Read RXB0 data */
    uint8_t sidh = mcp_read(MCP2515_RXB0SIDH);
    uint8_t sidl = mcp_read(0x62);  /* RXB0SIDL */
    frame->id = ((uint16_t)sidh << 3) | (sidl >> 5);

    uint8_t dlc = mcp_read(MCP2515_RXB0DLC) & 0x0F;
    frame->dlc = (dlc > 8) ? 8 : dlc;

    for (uint8_t i = 0; i < frame->dlc; i++) {
        frame->data[i] = mcp_read(MCP2515_RXB0D0 + i);
    }

    /* Clear RX0IF interrupt flag */
    mcp_bit_modify(MCP2515_CANINTF, 0x01, 0x00);

    return true;
}

bool can_available(void)
{
    return (mcp_read(MCP2515_CANINTF) & 0x01) != 0;
}
/**
 * AsthmaSync — SX1262 Radio Driver Wrapper (Implementation)
 *
 * This is a stub implementation that mirrors the Semtech SX126x API.
 * In production, link against the official sx126x driver and implement
 * the HAL functions (SPI read/write, DIO IRQ handling) for your platform.
 *
 * License: MIT
 */

#include "radio.h"
#include <string.h>

/* ── Internal State ─────────────────────────────────────── */
static radio_state_t s_state = RADIO_STATE_IDLE;
static int8_t s_last_rssi = 0;
static int8_t s_last_snr  = 0;
static uint8_t s_tdma_slot = 0;
static uint8_t s_seq = 0;

/* ── Stub HAL (platform-specific) ───────────────────────── */
/* In production these call into the Semtech SX126x HAL. */

static int hal_spi_write(uint8_t reg, const uint8_t *data, size_t len)
{
    (void)reg; (void)data; (void)len;
    return 0;  /* TODO: implement for ESP32-S3 (HSPI) or nRF52840 (SPIM) */
}

static int hal_spi_read(uint8_t reg, uint8_t *data, size_t len)
{
    (void)reg; (void)data; (void)len;
    return 0;  /* TODO */
}

static void hal_reset(void)
{
    /* Toggle SX1262 RESET pin low for 100 µs then release */
}

static void hal_delay_ms(uint32_t ms)
{
    /* vTaskDelay(pdMS_TO_TICKS(ms)) on FreeRTOS, or esp_rom_delay_us */
    (void)ms;
}

/* ── Radio API ──────────────────────────────────────────── */

int radio_init(void)
{
    hal_reset();
    hal_delay_ms(10);

    /* Set Standby mode */
    hal_spi_write(0x00, (uint8_t[]){0x00}, 1);  /* STDBY_RC */

    /* Set packet type: LoRa (0x01) */
    hal_spi_write(0x01, (uint8_t[]){0x01}, 1);

    /* Set frequency: 868 MHz
       freq = (868000000 / 32e6) << 25 = 0x6C8000 (32-bit) */
    uint32_t freq_reg = (uint32_t)((double)RADIO_FREQ_HZ / 32000000.0 * (1 << 25));
    uint8_t freq_buf[4] = {
        (freq_reg >> 24) & 0xFF,
        (freq_reg >> 16) & 0xFF,
        (freq_reg >> 8)  & 0xFF,
        freq_reg & 0xFF
    };
    hal_spi_write(0x02, freq_buf, 4);

    /* Set modulation params: SF7, BW 125 kHz, CR 4/5 */
    uint8_t mod_params[3] = { RADIO_SF, 0x04 /*125 kHz*/, RADIO_CR - 4 };
    hal_spi_write(0x8B, mod_params, 3);

    /* Set TX power: +14 dBm */
    uint8_t pa_cfg[2] = { 0x00 /*paDutyCycle*/, 0x00 /*hpMax*/ };
    hal_spi_write(0x95, pa_cfg, 2);

    s_state = RADIO_STATE_IDLE;
    s_seq = 0;
    return 0;
}

int radio_send(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > 255)
        return -1;

    /* Set TX buffer base address */
    uint8_t base_addr[2] = { 0x00, 0x00 };
    hal_spi_write(0x8F, base_addr, 2);

    /* Write payload to TX buffer */
    hal_spi_write(0x0E, data, len);  /* WriteBuffer */

    /* Set payload length */
    uint8_t plen[1] = { (uint8_t)len };
    hal_spi_write(0x90, plen, 1);

    /* Set TX mode (with timeout) */
    uint32_t timeout = RADIO_TX_TIMEOUT_MS * 64; /* SX1262 timeout unit = 15.625 µs */
    uint8_t tx_cmd[3] = {
        (timeout >> 16) & 0xFF,
        (timeout >> 8)  & 0xFF,
        timeout & 0xFF
    };
    hal_spi_write(0x83, tx_cmd, 3);  /* SetTx */

    s_state = RADIO_STATE_TX;

    /* Wait for TxDone IRQ (in production: DIO1 interrupt) */
    hal_delay_ms(RADIO_TX_TIMEOUT_MS);

    s_state = RADIO_STATE_IDLE;
    s_seq++;
    return 0;
}

int radio_recv(uint8_t *buf, size_t buf_size, int8_t *rssi, int8_t *snr)
{
    if (!buf || buf_size == 0)
        return -1;

    /* In production: check DIO1 for RxDone IRQ.
       Here we just return 0 (no packet). */
    (void)buf; (void)buf_size;

    if (rssi) *rssi = s_last_rssi;
    if (snr)  *snr  = s_last_snr;
    return 0;  /* no packet available */
}

int radio_rx_start(uint32_t timeout_ms)
{
    uint32_t timeout = timeout_ms * 64;
    uint8_t rx_cmd[3] = {
        (timeout >> 16) & 0xFF,
        (timeout >> 8)  & 0xFF,
        timeout & 0xFF
    };
    hal_spi_write(0x82, rx_cmd, 3);  /* SetRx */
    s_state = RADIO_STATE_RX;
    return 0;
}

int radio_sleep(void)
{
    hal_spi_write(0x84, (uint8_t[]){0x04}, 1);  /* SetSleep */
    s_state = RADIO_STATE_SLEEP;
    return 0;
}

int radio_wakeup(void)
{
    hal_reset();
    hal_delay_ms(10);
    s_state = RADIO_STATE_IDLE;
    return 0;
}

void radio_get_stats(int8_t *rssi, int8_t *snr)
{
    if (rssi) *rssi = s_last_rssi;
    if (snr)  *snr  = s_last_snr;
}

/* ── TDMA (Hub coordinator) ─────────────────────────────── */

int tdma_hub_superframe(void)
{
    /* Slot 0: Send beacon with timestamp + slot assignments */
    pkt_header_t beacon = {0};
    uint8_t beacon_payload[4] = {
        TDMA_SUPERFRAME_MS & 0xFF,
        (TDMA_SUPERFRAME_MS >> 8) & 0xFF,
        s_seq,
        0x00  /* beacon marker */
    };

    beacon.src_type = NODE_TYPE_HUB;
    beacon.src_id   = 0x0001;
    beacon.msg_type = MSG_TYPE_TIME_SYNC;
    beacon.seq      = s_seq++;

    uint8_t tx_buf[PKT_MAX_SIZE];
    size_t tx_len = proto_pack(&beacon, beacon_payload, 4, tx_buf, sizeof(tx_buf));
    if (tx_len == 0)
        return -1;

    radio_send(tx_buf, tx_len);

    /* Slots 1-N: Listen for node packets */
    for (int slot = 1; slot <= TDMA_MAX_NODES; slot++) {
        radio_rx_start(TDMA_SLOT_MS - TDMA_GUARD_MS);
        hal_delay_ms(TDMA_SLOT_MS);
    }

    return 0;
}

int tdma_assign_slot(uint16_t node_id, uint8_t *out_slot)
{
    /* Find next free slot */
    static uint8_t next_slot = 1;

    if (next_slot >= TDMA_MAX_NODES) {
        next_slot = 1;  /* wrap (or reject) */
    }

    *out_slot = next_slot++;
    return 0;
}

/* ── TDMA (Node side) ───────────────────────────────────── */

int tdma_node_sync(uint32_t timeout_ms)
{
    /* Listen for hub beacon */
    uint8_t rx_buf[PKT_MAX_SIZE];
    pkt_header_t hdr;
    uint16_t payload_len;

    radio_rx_start(timeout_ms);

    /* Poll for beacon */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        int n = radio_recv(rx_buf, sizeof(rx_buf), NULL, NULL);
        if (n > 0) {
            if (proto_unpack(rx_buf, n, &hdr, NULL, &payload_len) == 0) {
                if (hdr.src_type == NODE_TYPE_HUB &&
                    hdr.msg_type == MSG_TYPE_TIME_SYNC) {
                    return 0;  /* synchronized */
                }
            }
        }
        hal_delay_ms(100);
        elapsed += 100;
    }
    return -1;  /* sync failed */
}

int tdma_node_send(const uint8_t *data, size_t len)
{
    /* Wait until our slot, then transmit */
    uint32_t slot_offset = s_tdma_slot * TDMA_SLOT_MS;
    hal_delay_ms(slot_offset);

    return radio_send(data, len);
}

uint8_t tdma_get_slot(void)
{
    return s_tdma_slot;
}
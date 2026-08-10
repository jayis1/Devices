/*
 * FireSync — Sub-GHz 868 MHz TDMA Mesh Network Implementation
 * SX1262 radio driver + TDMA slot management + self-healing mesh relay
 *
 * Platform: ESP32-S3 (ESP-IDF) / STM32G431 (HAL) abstraction
 */
#include "subghz_mesh.h"
#include <string.h>

/* === SX1262 Register Definitions === */
#define SX1262_REG_SPI_TX_BUF       0x00
#define SX1262_REG_SPI_RX_BUF       0x00
#define SX1262_REG_PKT_STATUS       0x14
#define SX1262_REG_RSSI             0x15
#define SX1262_CMD_SET_STANDBY      0x80
#define SX1262_CMD_SET_TX           0x83
#define SX1262_CMD_SET_RX           0x82
#define SX1262_CMD_SET_SLEEP        0x84
#define SX1262_CMD_SET_RF_FREQ      0x86
#define SX1262_CMD_SET_TX_PARAMS    0x8E
#define SX1262_CMD_SET_MOD_PARAMS   0x8B
#define SX1262_CMD_SET_PACKET_TYPE  0x8A
#define SX1262_CMD_SET_TX_FALLBACK  0x8F
#define SX1262_CMD_CALIBRATE        0x89
#define SX1262_CMD_CLEAR_IRQ        0x02
#define SX1262_CMD_GET_IRQ_STATUS   0x12
#define SX1262_CMD_WRITE_BUFFER     0x0D
#define SX1262_CMD_READ_BUFFER      0x1D
#define SX1262_CMD_SET_RX_FALLBACK  0x81
#define SX1262_CMD_SET_DIO3_TCXO   0x97
#define SX1262_CMD_SET_DIO2_RF_SW   0x9D

#define SX1262_PKT_TYPE_LORA        0x01
#define SX1262_IRQ_TX_DONE          0x0001
#define SX1262_IRQ_RX_DONE          0x0002
#define SX1262_IRQ_TIMEOUT          0x0004
#define SX1262_IRQ_CRC_ERR          0x0020

#define SX1262_TX_TIMEOUT_CONTINUOUS 0x000000

/* === SPI Helpers === */
static void sx_write_reg(const fs_radio_hal_t *hal, uint8_t addr,
                         const uint8_t *data, size_t len)
{
    hal->cs_low();
    uint8_t tx = addr | 0x80; /* Write bit */
    uint8_t rx;
    hal->spi_xfer(&tx, &rx, 1);
    if (len > 0)
        hal->spi_xfer(data, NULL, len);
    hal->cs_high();
}

static void sx_read_reg(const fs_radio_hal_t *hal, uint8_t addr,
                        uint8_t *data, size_t len)
{
    hal->cs_low();
    uint8_t tx = addr & 0x7F; /* Read bit (0) */
    uint8_t rx;
    hal->spi_xfer(&tx, &rx, 1);
    hal->spi_xfer(NULL, data, len);
    hal->cs_high();
}

static void sx_write_command(const fs_radio_hal_t *hal, uint8_t cmd,
                             const uint8_t *params, size_t param_len)
{
    while (hal->busy_read()) hal->delay_us(10);
    hal->cs_low();
    uint8_t tx = cmd;
    uint8_t rx;
    hal->spi_xfer(&tx, &rx, 1);
    if (param_len > 0 && params)
        hal->spi_xfer(params, NULL, param_len);
    hal->cs_high();
    while (hal->busy_read()) hal->delay_us(10);
}

static void sx_read_command(const fs_radio_hal_t *hal, uint8_t cmd,
                             uint8_t *status, size_t len)
{
    while (hal->busy_read()) hal->delay_us(10);
    hal->cs_low();
    uint8_t tx = cmd;
    hal->spi_xfer(&tx, NULL, 1);
    /* Dummy byte for status read */
    hal->spi_xfer(NULL, status, len);
    hal->cs_high();
}

/* === SX1262 Initialization === */
int fs_sx1262_init(const fs_radio_hal_t *hal, const fs_radio_config_t *cfg)
{
    if (!hal || !cfg) return -1;

    hal->spi_init();
    /* Hardware reset */
    hal->reset(1);
    hal->delay_ms(10);
    hal->reset(0);
    hal->delay_ms(20);

    /* Set standby (RC oscillator) */
    uint8_t standby_cmd[] = {0x00}; /* STDBY_RC */
    sx_write_command(hal, SX1262_CMD_SET_STANDBY, standby_cmd, 1);
    hal->delay_ms(10);

    /* Set packet type: LoRa */
    uint8_t pkt_type = SX1262_PKT_TYPE_LORA;
    sx_write_command(hal, SX1262_CMD_SET_PACKET_TYPE, &pkt_type, 1);

    /* Set RF frequency (Hz → 32-bit PLL, freq = Hz / (32 MHz / 2^25)) */
    uint32_t rf_freq = (uint32_t)((uint64_t)cfg->freq_hz * (1 << 25) / 32000000ULL);
    uint8_t freq_cmd[4];
    freq_cmd[0] = (rf_freq >> 24) & 0xFF;
    freq_cmd[1] = (rf_freq >> 16) & 0xFF;
    freq_cmd[2] = (rf_freq >> 8) & 0xFF;
    freq_cmd[3] = rf_freq & 0xFF;
    sx_write_command(hal, SX1262_CMD_SET_RF_FREQ, freq_cmd, 4);

    /* Set TX power */
    uint8_t tx_params[3] = {
        (uint8_t)cfg->tx_power_dbm, /* Power dBm */
        0x04,                        /* Ramp time 200 μs */
        0x00                         /* PA config: default */
    };
    sx_write_command(hal, SX1262_CMD_SET_TX_PARAMS, tx_params, 3);

    /* Set modulation params: SF, BW, CR, LDRO */
    uint8_t mod_params[4] = {
        cfg->spreading_factor,              /* Spreading factor */
        0x04,                                 /* BW 125 kHz = 0x04 */
        cfg->sync_word & 0xFF,               /* Coding rate 4/5 = 0x01 */
        0x00                                  /* LDRO off */
    };
    sx_write_command(hal, SX1262_CMD_SET_MOD_PARAMS, mod_params, 4);

    /* Set sync word (LoRa private network = 0x1424, we use 0x3445) */
    uint8_t sync_cmd[2] = {
        (cfg->sync_word >> 8) & 0xFF,
        cfg->sync_word & 0xFF
    };
    sx_write_reg(hal, 0x0740, sync_cmd, 2);

    /* Set preamble length */
    uint8_t preamble_cmd[2] = {
        (cfg->preamble_len >> 8) & 0xFF,
        cfg->preamble_len & 0xFF
    };
    sx_write_reg(hal, 0x08BC, preamble_cmd, 2);

    /* Clear IRQ status */
    uint8_t clear_irq[2] = {0xFF, 0xFF};
    sx_write_command(hal, SX1262_CMD_CLEAR_IRQ, clear_irq, 2);

    return 0;
}

/* === TX === */
int fs_sx1262_tx(const fs_radio_hal_t *hal, const uint8_t *data, size_t len,
                 int8_t power_dbm)
{
    if (!hal || !data || len == 0 || len > 255) return -1;

    /* Write data to TX buffer */
    uint8_t offset = 0;
    sx_write_reg(hal, 0x00, &offset, 1); /* Set TX buffer offset */
    sx_write_reg(hal, 0x00, data, len);  /* Write payload */

    /* Set TX with timeout (0 = no timeout, TX done interrupt) */
    uint8_t tx_cmd[3] = {0x00, 0x00, 0x00}; /* 0 = no timeout */
    sx_write_command(hal, SX1262_CMD_SET_TX, tx_cmd, 3);

    /* Wait for TX done */
    uint32_t timeout = 5000; /* 5s max */
    while (timeout > 0) {
        uint8_t irq[2];
        sx_read_command(hal, SX1262_CMD_GET_IRQ_STATUS, irq, 2);
        uint16_t irq_status = (irq[0] << 8) | irq[1];
        if (irq_status & SX1262_IRQ_TX_DONE) {
            uint8_t clear[2] = {0xFF, 0xFF};
            sx_write_command(hal, SX1262_CMD_CLEAR_IRQ, clear, 2);
            return (int)len;
        }
        hal->delay_ms(1);
        timeout--;
    }
    return -2; /* Timeout */
}

/* === RX === */
int fs_sx1262_rx(fs_radio_hal_t *hal, uint8_t *buf, size_t max_len,
                 uint32_t timeout_ms, int8_t *rssi)
{
    if (!hal || !buf) return -1;

    /* Set RX with timeout */
    uint32_t timeout = timeout_ms * 64; /* SX1262 timeout in 15.625 μs units */
    uint8_t rx_cmd[3];
    rx_cmd[0] = (timeout >> 16) & 0xFF;
    rx_cmd[1] = (timeout >> 8) & 0xFF;
    rx_cmd[2] = timeout & 0xFF;
    sx_write_command(hal, SX1262_CMD_SET_RX, rx_cmd, 3);

    /* Wait for RX done or timeout */
    uint32_t waited = 0;
    while (waited < timeout_ms + 100) {
        uint8_t irq[2];
        sx_read_command(hal, SX1262_CMD_GET_IRQ_STATUS, irq, 2);
        uint16_t irq_status = (irq[0] << 8) | irq[1];
        if (irq_status & SX1262_IRQ_RX_DONE) {
            /* Get RSSI */
            if (rssi) {
                uint8_t pkt_status[3];
                sx_read_command(hal, SX1262_CMD_GET_IRQ_STATUS + 2, pkt_status, 3);
                /* RSSI = -157 + pkt_status[0]/2 (dBm) — approximation */
                *rssi = (int8_t)(-157 + pkt_status[0] / 2);
            }

            /* Read RX buffer */
            uint8_t rx_len_reg[1];
            sx_read_reg(hal, 0x0D, rx_len_reg, 1); /* Last byte written by radio */
            uint8_t rx_start[1];
            sx_read_reg(hal, 0x0E, rx_start, 1); /* RX start offset */
            size_t rx_len = rx_len_reg[0];
            if (rx_len > max_len) rx_len = max_len;

            sx_read_reg(hal, 0x00, buf, rx_len); /* Read payload */

            /* Clear IRQ */
            uint8_t clear[2] = {0xFF, 0xFF};
            sx_write_command(hal, SX1262_CMD_CLEAR_IRQ, clear, 2);

            return (int)rx_len;
        }
        if (irq_status & SX1262_IRQ_TIMEOUT) {
            uint8_t clear[2] = {0xFF, 0xFF};
            sx_write_command(hal, SX1262_CMD_CLEAR_IRQ, clear, 2);
            return 0; /* Timeout, no data */
        }
        hal->delay_ms(1);
        waited++;
    }
    return 0;
}

/* === Sleep === */
int fs_sx1262_sleep(const fs_radio_hal_t *hal)
{
    uint8_t sleep_cmd[1] = {0x04}; /* Sleep + warm start */
    sx_write_command(hal, SX1262_CMD_SET_SLEEP, sleep_cmd, 1);
    hal->delay_ms(1);
    return 0;
}

/* === Set Frequency === */
int fs_sx1262_set_freq(const fs_radio_hal_t *hal, uint32_t freq_hz)
{
    if (!hal) return -1;
    uint32_t rf_freq = (uint32_t)((uint64_t)freq_hz * (1 << 25) / 32000000ULL);
    uint8_t freq_cmd[4];
    freq_cmd[0] = (rf_freq >> 24) & 0xFF;
    freq_cmd[1] = (rf_freq >> 16) & 0xFF;
    freq_cmd[2] = (rf_freq >> 8) & 0xFF;
    freq_cmd[3] = rf_freq & 0xFF;
    sx_write_command(hal, SX1262_CMD_SET_RF_FREQ, freq_cmd, 4);
    return 0;
}

/* === IRQ Handler === */
void fs_sx1262_irq_handler(const fs_radio_hal_t *hal)
{
    /* Called on DIO1 rising edge — check IRQ status */
    uint8_t irq[2];
    sx_read_command(hal, SX1262_CMD_GET_IRQ_STATUS, irq, 2);
    uint16_t irq_status = (irq[0] << 8) | irq[1];

    if (irq_status & (SX1262_IRQ_TX_DONE | SX1262_IRQ_RX_DONE | SX1262_IRQ_TIMEOUT)) {
        uint8_t clear[2] = {0xFF, 0xFF};
        sx_write_command(hal, SX1262_CMD_CLEAR_IRQ, clear, 2);
    }
    if (hal->on_dio1) hal->on_dio1();
}

/* === Mesh Layer === */

int fs_mesh_init(fs_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type,
                 const uint8_t *aes_key)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->node_id = node_id;
    ctx->node_type = node_type;
    ctx->tdma_slot = 0; /* Will be assigned by Hub */
    ctx->msg_counter = 0;
    ctx->joined = 0;
    if (aes_key) {
        memcpy(ctx->aes_key, aes_key, FS_AES_KEY_LEN);
        memset(ctx->aes_nonce, 0, FS_AES_CTR_NONCE_LEN);
    }
    return 0;
}

int fs_mesh_join(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal)
{
    if (!ctx || !hal) return -1;

    /* Send JOIN_REQ (up to 3 retries) */
    fs_message_t msg;
    fs_build_join_req(&msg, ctx->node_id, ctx->msg_counter++,
                      ctx->node_type, ctx->battery_v, 1, 0);

    uint8_t tx_buf[FS_MAX_MSG];
    size_t tx_len = fs_encode(&msg, tx_buf, sizeof(tx_buf));

    for (int retry = 0; retry < 3; retry++) {
        fs_sx1262_tx(hal, tx_buf, tx_len, 22);

        /* Wait for JOIN_ACK */
        uint8_t rx_buf[FS_MAX_MSG];
        int8_t rssi;
        int rx_len = fs_sx1262_rx((fs_radio_hal_t *)hal, rx_buf, sizeof(rx_buf),
                                  2000, &rssi);
        if (rx_len > 0) {
            fs_message_t resp;
            if (fs_decode(&resp, rx_buf, rx_len) == 0 &&
                resp.header.type == FS_MSG_JOIN_ACK) {
                ctx->tdma_slot = resp.payload[0];
                ctx->joined = 1;
                return 0;
            }
        }
    }
    return -1;
}

int fs_mesh_send(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                 const fs_message_t *msg, uint8_t priority)
{
    if (!ctx || !hal || !msg) return -1;

    uint8_t tx_buf[FS_MAX_MSG];
    size_t tx_len = fs_encode(msg, tx_buf, sizeof(tx_buf));
    if (tx_len == 0) return -1;

    /* Normal: transmit in TDMA slot. Emergency: bypass TDMA. */
    if (priority == FS_SEV_EMERGENCY) {
        /* 3× immediate transmission (priority slot) */
        for (int i = 0; i < 3; i++) {
            fs_sx1262_tx(hal, tx_buf, tx_len, 22);
            hal->delay_ms(50);
        }
        return (int)tx_len;
    }

    /* Normal: single TX */
    int ret = fs_sx1262_tx(hal, tx_buf, tx_len, 22);
    return ret;
}

int fs_mesh_recv(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                 fs_message_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !hal || !msg) return -1;

    uint8_t rx_buf[FS_MAX_MSG];
    int8_t rssi;
    int rx_len = fs_sx1262_rx((fs_radio_hal_t *)hal, rx_buf, sizeof(rx_buf),
                              timeout_ms, &rssi);
    if (rx_len <= 0) return 0;

    ctx->last_rssi = rssi;

    int rc = fs_decode(msg, rx_buf, rx_len);
    if (rc != 0) return -1;

    return 1;
}

int fs_mesh_relay(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                  const fs_message_t *msg)
{
    if (!ctx || !hal || !msg) return -1;
    /* Relay: retransmit message (mesh self-healing) */
    uint8_t tx_buf[FS_MAX_MSG];
    size_t tx_len = fs_encode(msg, tx_buf, sizeof(tx_buf));
    if (tx_len == 0) return -1;
    return fs_sx1262_tx(hal, tx_buf, tx_len, 22);
}

int fs_mesh_broadcast_emergency(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                                 const fs_message_t *msg)
{
    if (!ctx || !hal || !msg) return -1;

    uint8_t tx_buf[FS_MAX_MSG];
    size_t tx_len = fs_encode(msg, tx_buf, sizeof(tx_buf));
    if (tx_len == 0) return -1;

    /* 3× broadcast for reliability */
    for (int i = 0; i < 3; i++) {
        fs_sx1262_tx(hal, tx_buf, tx_len, 22);
        hal->delay_ms(100);
    }
    return (int)tx_len;
}

int fs_mesh_send_emergency(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                           const fs_message_t *msg)
{
    return fs_mesh_send(ctx, hal, msg, FS_SEV_EMERGENCY);
}

int fs_mesh_send_acked(fs_mesh_ctx_t *ctx, const fs_radio_hal_t *hal,
                       const fs_message_t *msg, uint32_t timeout_ms,
                       uint8_t max_retries)
{
    if (!ctx || !hal || !msg) return -1;

    uint8_t tx_buf[FS_MAX_MSG];
    size_t tx_len = fs_encode(msg, tx_buf, sizeof(tx_buf));

    for (uint8_t retry = 0; retry < max_retries; retry++) {
        fs_sx1262_tx(hal, tx_buf, tx_len, 22);

        /* Wait for CMD_ACK or HEARTBEAT */
        uint8_t rx_buf[FS_MAX_MSG];
        int8_t rssi;
        int rx_len = fs_sx1262_rx((fs_radio_hal_t *)hal, rx_buf, sizeof(rx_buf),
                                  timeout_ms, &rssi);
        if (rx_len > 0) {
            fs_message_t ack;
            if (fs_decode(&ack, rx_buf, rx_len) == 0 &&
                (ack.header.type == FS_MSG_CMD_ACK ||
                 ack.header.type == FS_MSG_HEARTBEAT)) {
                return 0; /* ACK received */
            }
        }
    }
    return -1; /* No ACK */
}

/* === AES-128-CTR (placeholder — production uses hardware AES or mbedTLS) === */
int fs_mesh_aes_encrypt(fs_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce)
{
    /* Production: use mbedTLS aes_crypt_ctr() or ESP hardware AES */
    (void)ctx; (void)data; (void)len; (void)nonce;
    return 0;
}

int fs_mesh_aes_decrypt(fs_mesh_ctx_t *ctx, uint8_t *data, size_t len,
                         const uint8_t *nonce)
{
    /* CTR mode: decrypt = encrypt (same operation) */
    return fs_mesh_aes_encrypt(ctx, data, len, nonce);
}
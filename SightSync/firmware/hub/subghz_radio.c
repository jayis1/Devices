/**
 * SightSync Vision Hub — Sub-GHz Radio (CC1101 868 MHz) Implementation
 *
 * Uses TDMA: hub broadcasts heartbeat at T+0, desk at T+100ms,
 * lamp at T+200ms. GFSK, 38.4 kbaud.
 *
 * License: MIT
 */

#include "subghz_radio.h"
#include "../common/crc8.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "subghz";
static subghz_rx_cb_t s_rx_cb = NULL;

/* CC1101 register definitions */
#define CC1101_IOCFG2   0x00
#define CC1101_IOCFG0   0x02
#define CC1101_FIFOTHR  0x03
#define CC1101_PKTLEN   0x06
#define CC1101_PKTCTRL1 0x07
#define CC1101_PKTCTRL0 0x08
#define CC1101_ADDR     0x09
#define CC1101_CHANNR   0x0A
#define CC1101_FSCTRL1  0x0B
#define CC1101_FREQ2    0x0D
#define CC1101_FREQ1    0x0E
#define CC1101_FREQ0    0x0F
#define CC1101_MDMCFG4  0x10
#define CC1101_MDMCFG3  0x11
#define CC1101_MDMCFG2  0x12
#define CC1101_MDMCFG1  0x13
#define CC1101_MDMCFG0  0x14
#define CC1101_DEVIATN  0x15
#define CC1101_MCSM0    0x18
#define CC1101_FOCCFG   0x19
#define CC1101_BSCFG    0x1A
#define CC1101_AGCCTRL2 0x1B
#define CC1101_WORCTRL  0x1F
#define CC1101_FSCAL3   0x21
#define CC1101_FSCAL2   0x22
#define CC1101_FSCAL1   0x23
#define CC1101_FSCAL0   0x24
#define CC1101_TEST2    0x2C
#define CC1101_TEST1    0x2D
#define CC1101_TEST0    0x2E
#define CC1101_SRX      0x34
#define CC1101_STX      0x35
#define CC1101_SIDLE    0x36
#define CC1101_SFRX     0x3A
#define CC1101_SFTX     0x3B
#define CC1101_SWORRST  0x3C
#define CC1101_SNOP     0x3D
#define CC1101_TXFIFO   0x3F
#define CC1101_RXFIFO   0x3F
#define CC1101_TXBYTES  0x3A
#define CC1101_RXBYTES  0x3B
#define CC1101_PKTSTATUS 0x38
#define CC1101_MARCSTATE 0x35

#define CC1101_WRITE   0x00
#define CC1101_READ    0x80
#define CC1101_BURST   0x40

static spi_device_handle_t s_spi;
static bool s_initialized = false;

/* ── CC1101 SPI helpers ──────────────────────────────────────────── */

static void cc1101_write_reg(uint8_t addr, uint8_t val)
{
    spi_transaction_t t = {0};
    uint16_t tx = (addr | CC1101_WRITE) | ((uint16_t)val << 8);
    t.tx_data[0] = tx & 0xFF;
    t.tx_data[1] = (tx >> 8) & 0xFF;
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(s_spi, &t);
}

static uint8_t cc1101_read_reg(uint8_t addr)
{
    spi_transaction_t t = {0};
    t.tx_data[0] = addr | CC1101_READ;
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(s_spi, &t);
    return t.rx_data[1];
}

static void cc1101_reset(void)
{
    /* SFRX + SIDLE + SWORRST */
    cc1101_write_reg(CC1101_SFRX, 0);
    cc1101_write_reg(CC1101_SIDLE, 0);
}

/* ── CC1101 Configuration for 868 MHz, 38.4 kbaud GFSK ───────────── */

static void cc1101_configure(void)
{
    /* Frequency: 868.0 MHz
     * FREQ = 868.0 / 26.0 * 2^16 = 0x21656A (approx)
     * Actually: 868.0 = 26.0 MHz * (FREQ/2^16)
     * FREQ = 868.0 / 26.0 * 65536 = 2187158.4 → 0x21656B
     */
    cc1101_write_reg(CC1101_FREQ2, 0x21);
    cc1101_write_reg(CC1101_FREQ1, 0x65);
    cc1101_write_reg(CC1101_FREQ0, 0x6B);

    /* Modem config: GFSK, 38.4 kbaud */
    cc1101_write_reg(CC1101_MDMCFG4, 0xCA);  /* DRATE_E=10, CHANBW_E=2, CHANBW_M=0 */
    cc1101_write_reg(CC1101_MDMCFG3, 0x83);  /* DRATE_M=131 */
    cc1101_write_reg(CC1101_MDMCFG2, 0x13);  /* GFSK, 16/16 sync, CRC enable, manchester off */
    cc1101_write_reg(CC1101_MDMCFG1, 0x22);  /* FEC off, 2 preamble bytes, CHANSPC_E=2 */
    cc1101_write_reg(CC1101_MDMCFG0, 0xF8);  /* CHANSPC_M=248 */
    cc1101_write_reg(CC1101_DEVIATN, 0x35);  /* DEV_E=5, DEV_M=5 → ±19.0 kHz */

    /* Packet config: variable length, CRC, address check off */
    cc1101_write_reg(CC1101_PKTCTRL0, 0x05); /* variable length, CRC, no whitening */
    cc1101_write_reg(CC1101_PKTLEN, SS_MAX_PACKET_LEN);

    /* IO config: GDO0 = packet RX/TX interrupt */
    cc1101_write_reg(CC1101_IOCFG0, 0x06);

    /* MCSM0: auto-calibrate on transition to RX/TX */
    cc1101_write_reg(CC1101_MCSM0, 0x18);

    /* Frequency synthesis calibration */
    cc1101_write_reg(CC1101_FSCAL3, 0xEA);
    cc1101_write_reg(CC1101_FSCAL2, 0x2A);
    cc1101_write_reg(CC1101_FSCAL1, 0x00);
    cc1101_write_reg(CC1101_FSCAL0, 0x11);

    ESP_LOGI(TAG, "CC1101 configured for 868.0 MHz, 38.4 kbaud GFSK");
}

/* ── RX interrupt handler (GDO0) ──────────────────────────────────── */

static void IRAM_ATTR gdo0_isr(void *arg)
{
    (void)arg;
    /* Read packet from RX FIFO and decode */
    uint8_t rx_bytes = cc1101_read_reg(CC1101_RXBYTES);
    if (rx_bytes > 0) {
        uint8_t pkt_len = cc1101_read_reg(CC1101_RXFIFO);
        if (pkt_len > 0 && pkt_len <= SS_MAX_PACKET_LEN) {
            uint8_t buf[SS_MAX_PACKET_LEN];
            /* Read packet from RX FIFO */
            for (uint8_t i = 0; i < pkt_len; i++) {
                buf[i] = cc1101_read_reg(CC1101_RXFIFO);
            }

            sightsync_header_t hdr;
            const uint8_t *payload = NULL;
            if (sightsync_decode(buf, pkt_len, &hdr, &payload)) {
                if (s_rx_cb != NULL) {
                    s_rx_cb(&hdr, payload);
                }
            }
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void subghz_radio_init(subghz_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;

    /* Initialize SPI bus for CC1101 */
    spi_bus_config_t buscfg = {
        .miso_io_num = 19,
        .mosi_io_num = 20,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 0);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,  /* 5 MHz */
        .mode = 0,
        .spics_io_num = 17,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

    /* Configure GDO0 interrupt */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 21),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(21, gdo0_isr, NULL);

    /* Reset and configure CC1101 */
    cc1101_reset();
    cc1101_configure();

    /* Enter RX mode */
    cc1101_write_reg(CC1101_SRX, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "Sub-GHz radio initialized (868 MHz)");
}

void subghz_radio_send(uint16_t dest_id, const uint8_t *data, uint8_t len)
{
    if (!s_initialized) return;

    /* Enter IDLE, flush TX FIFO, write packet, enter TX */
    cc1101_write_reg(CC1101_SIDLE, 0);
    cc1101_write_reg(CC1101_SFTX, 0);

    /* Write packet length + data to TX FIFO */
    cc1101_write_reg(CC1101_TXFIFO, len);
    for (uint8_t i = 0; i < len; i++) {
        cc1101_write_reg(CC1101_TXFIFO, data[i]);
    }

    /* Enter TX mode */
    cc1101_write_reg(CC1101_STX, 0);

    /* Wait for TX to complete (poll GDO0) */
    /* In production, use interrupt + task notification */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Return to RX */
    cc1101_write_reg(CC1101_SRX, 0);

    (void)dest_id;
}

bool subghz_radio_is_ready(void)
{
    return s_initialized;
}
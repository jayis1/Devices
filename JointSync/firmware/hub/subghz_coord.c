/**
 * JointSync Hub — Sub-GHz 868 MHz TDMA Coordinator
 *
 * CC1120 transceiver, TDMA with 8 slots × 50 ms = 400 ms cycle.
 * Slot 0: Hub beacon, Slots 1-6: Sleeve data, Slot 7: contention.
 *
 * License: MIT
 */

#include "subghz_coord.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "subghz_coord";

/* CC1120 SPI Configuration */
#define CC1120_HOST    SPI2_HOST
#define CC1120_CS_PIN  1
#define CC1120_CLK_PIN 2
#define CC1120_MISO    3
#define CC1120_MOSI    4
#define CC1120_GPIO0   5   /* IRQ */
#define CC1120_RESET   6

static spi_device_handle_t g_spi;
static subghz_data_cb_t g_callback = NULL;
static bool g_running = false;

/* CC1120 Register Addresses (extended) */
#define CC1120_IOCFG3        0x00
#define CC1120_IOCFG2        0x01
#define CC1120_IOCFG1        0x02
#define CC1120_IOCFG0        0x03
#define CC1120_SYNC_CFG1     0x06
#define CC1120_SYNC_CFG0     0x07
#define CC1120_DEVIATION_M   0x08
#define CC1120_MODCFG_DEV_E  0x09
#define CC1120_DCFILT_CFG    0x0A
#define CC1120_PREAMBLE_CFG0  0x0E
#define CC1120_PREAMBLE_CFG1  0x0F
#define CC1120_FREQ_IF_CFG   0x12
#define CC1120_FREQOFF_CFG   0x13
#define CC1120_FREQ2         0x1C
#define CC1120_FREQ1         0x1D
#define CC1120_FREQ0         0x1E
#define CC1120_CHAN_BW       0x1F
#define CC1120_MDMCFG0      0x20
#define CC1120_SYMBOL_RATE2 0x22
#define CC1120_SYMBOL_RATE1 0x23
#define CC1120_SYMBOL_RATE0 0x24
#define CC1120_AFC_CFG      0x27
#define CC1120_AGC_REF      0x2A
#define CC1120_AGC_CFG1     0x2B
#define CC1120_AGC_CFG0     0x2C
#define CC1120_FIFO_CFG     0x2F
#define CC1120_DEV_ADDR     0x35
#define CC1120_SETTLING_CFG 0x36
#define CC1120_FS_CFG       0x37
#define CC1120_WOR_CFG1     0x38
#define CC1120_WOR_CFG0     0x39
#define CC1120_WOR_TIME0    0x3C
#define CC1120_RX_CFG0      0x40
#define CC1120_RX_CFG1      0x41
#define CC1120_FIFO         0x3D
#define CC1120_TXFIRST       0x3E
#define CC1120_TXLAST       0x3F
#define CC1120_PKT_CFG1     0x46
#define CC1120_PKT_CFG0     0x47
#define CC1120_PKT_LEN      0x48
#define CC1120_IFAMP_CFG    0x4C
#define CC1120_TXFIFO       0x3F

/* CC1120 Strobe Commands */
#define CC1120_SRES         0x30
#define CC1120_SFSTXON      0x31
#define CC1120_SXOFF        0x32
#define CC1120_SCAL         0x33
#define CC1120_SRX          0x34
#define CC1120_STX          0x35
#define CC1120_SIDLE        0x36
#define CC1120_SWOR         0x38
#define CC1120_SPWD         0x39
#define CC1120_SFRX         0x3A
#define CC1120_SFTX         0x3B
#define CC1120_SNOP         0x3D

/* ── SPI Helpers ─────────────────────────────────────────────────── */

static esp_err_t cc1120_write_reg(uint16_t addr, uint8_t value)
{
    uint8_t tx[3];
    uint8_t rx[3];

    /* Address byte: bit7=R/W (1=write), bit6=burst, bit5=extended */
    if (addr & 0xFF00) {
        /* Extended register */
        tx[0] = 0x2F;  /* Extended + write + single */
        tx[1] = addr & 0xFF;
        tx[2] = value;
    } else {
        tx[0] = 0x00 | (addr & 0x3F);  /* Write + single */
        tx[1] = value;
        tx[2] = 0;
    }

    spi_transaction_t t = {0};
    t.length = (addr & 0xFF00) ? 24 : 16;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    return spi_device_polling_transmit(g_spi, &t);
}

static uint8_t cc1120_read_reg(uint16_t addr)
{
    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    if (addr & 0xFF00) {
        tx[0] = 0xAF;  /* Extended + read + single */
        tx[1] = addr & 0xFF;
    } else {
        tx[0] = 0x80 | (addr & 0x3F);  /* Read + single */
    }

    spi_transaction_t t = {0};
    t.length = (addr & 0xFF00) ? 24 : 16;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    spi_device_polling_transmit(g_spi, &t);
    return rx[1];
}

static esp_err_t cc1120_strobe(uint8_t cmd)
{
    uint8_t tx[1] = {cmd};
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = tx;
    return spi_device_polling_transmit(g_spi, &t);
}

/* ── CC1120 Initialization ───────────────────────────────────────── */

static esp_err_t cc1120_init(void)
{
    /* Hard reset */
    gpio_set_direction(CC1120_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(CC1120_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(CC1120_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Software reset */
    cc1120_strobe(CC1120_SRES);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Configure for 868 MHz, 2-FSK, 50 kbps */
    /* Frequency: 868.0 MHz (registers for CC1120 at 868 MHz) */
    cc1120_write_reg(CC1120_FREQ2, 0x6C);
    cc1120_write_reg(CC1120_FREQ1, 0x80);
    cc1120_write_reg(CC1120_FREQ0, 0x00);

    /* Symbol rate: 50 kbps */
    cc1120_write_reg(CC1120_SYMBOL_RATE2, 0x00);
    cc1120_write_reg(CC1120_SYMBOL_RATE1, 0x96);
    cc1120_write_reg(CC1120_SYMBOL_RATE0, 0x66);

    /* Modulation: 2-FSK, deviation 20 kHz */
    cc1120_write_reg(CC1120_MODCFG_DEV_E, 0x0C);
    cc1120_write_reg(CC1120_DEVIATION_M, 0x33);

    /* Packet config: variable length, sync word */
    cc1120_write_reg(CC1120_SYNC_CFG1, 0x0B);
    cc1120_write_reg(CC1120_SYNC_CFG0, 0x33);
    cc1120_write_reg(CC1120_PKT_CFG1, 0x03);
    cc1120_write_reg(CC1120_PKT_CFG0, 0x18);
    cc1120_write_reg(CC1120_PKT_LEN, 0xFF);

    /* FIFO config */
    cc1120_write_reg(CC1120_FIFO_CFG, 0x80);

    /* AGC config */
    cc1120_write_reg(CC1120_AGC_REF, 0x3C);
    cc1120_write_reg(CC1120_AGC_CFG1, 0x10);
    cc1120_write_reg(CC1120_AGC_CFG0, 0x10);

    /* RX config */
    cc1120_write_reg(CC1120_RX_CFG0, 0x1E);

    /* Settle / FS config */
    cc1120_write_reg(CC1120_SETTLING_CFG, 0x13);
    cc1120_write_reg(CC1120_FS_CFG, 0x12);

    /* Calibrate */
    cc1120_strobe(CC1120_SCAL);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "CC1120 initialized at 868 MHz, 50 kbps, 2-FSK");
    return ESP_OK;
}

/* ── Public API ───────────────────────────────────────────────────── */

void subghz_coord_init(subghz_data_cb_t callback)
{
    g_callback = callback;

    /* Initialize SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = CC1120_MOSI,
        .miso_io_num = CC1120_MISO,
        .sclk_io_num = CC1120_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CC1120_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,  /* 4 MHz */
        .mode = 0,
        .spics_io_num = CC1120_CS_PIN,
        .queue_size = 4,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(CC1120_HOST, &devcfg, &g_spi));

    /* Initialize GPIO for IRQ */
    gpio_set_direction(CC1120_GPIO0, GPIO_MODE_INPUT);
    gpio_set_intr_type(CC1120_GPIO0, GPIO_INTR_NEGEDGE);

    cc1120_init();
}

void subghz_coord_start(void)
{
    g_running = true;
    /* Enter RX mode */
    cc1120_strobe(CC1120_SRX);
    ESP_LOGI(TAG, "Sub-GHz coordinator started, RX mode");
}

void subghz_coord_stop(void)
{
    g_running = false;
    cc1120_strobe(CC1120_SIDLE);
    ESP_LOGI(TAG, "Sub-GHz coordinator stopped");
}

void subghz_send_packet(uint8_t *data, uint8_t len)
{
    if (!g_running || len > 128) return;

    /* Set TX FIFO */
    cc1120_write_reg(CC1120_TXFIRST, 0x00);

    /* Write packet to TX FIFO (burst write) */
    uint8_t header = 0x00 | CC1120_TXFIFO;
    spi_transaction_t t = {0};
    t.length = 8 + len * 8;
    t.tx_buffer = data;  /* Simplified: real impl uses burst write */
    spi_device_polling_transmit(g_spi, &t);

    /* Strobe TX */
    cc1120_strobe(CC1120_STX);

    /* Wait for TX completion (poll GPIO0) */
    int timeout = 100;
    while (gpio_get_level(CC1120_GPIO0) == 1 && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Return to RX */
    cc1120_strobe(CC1120_SRX);
}

/* ── TDMA Beacon Task ─────────────────────────────────────────────── */

void subghz_tdma_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t cycle = pdMS_TO_TICKS(400);  /* 400 ms TDMA cycle */

    while (g_running) {
        /* Slot 0: Send beacon */
        uint8_t beacon[4];
        beacon[0] = 0x4A;  /* Sync 'J' */
        beacon[1] = 0x53;  /* Sync 'S' */
        beacon[2] = g_conn_count;  /* Connected node count */
        beacon[3] = 0;     /* Reserved */

        subghz_send_packet(beacon, 4);

        /* Slots 1-6: Listen for sleeve data (50 ms each) */
        /* Already in RX mode from subghz_send_packet */

        /* Slot 7: Contention (pairing) */
        vTaskDelayUntil(&last_wake, cycle);
    }
}
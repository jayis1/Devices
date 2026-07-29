/*
 * RehabSync — Smart Band Node Firmware
 * nRF52840, FreeRTOS / nRF5 SDK
 *
 * The Smart Band measures exercise resistance force via HX711 24-bit ADC
 * + 50 kg load cell, tracks band orientation via LSM6DSO IMU for tempo
 * detection, and streams force + orientation to the Hub via BLE 5.0.
 *
 * Build: nrf5 build with nRF5 SDK v17.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "boards.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"
#include "nrf_saadc.h"
#include "ble.h"
#include "ble_nus.h"

#include "../common/protocol.h"
#include "../common/config.h"

#define TAG "RehabSync-SmartBand"

/* === HX711 Load Cell ADC Driver === */
#define HX711_GAIN_128         1
#define HX711_GAIN_64          3
#define HX711_GAIN_32          2

static int32_t g_hx711_offset = 0;    /* zero-offset (tare) */
static float g_hx711_scale = 1.0f;    /* calibration factor: counts → kg */
static uint8_t g_hx711_gain = HX711_GAIN_128;

static void hx711_init(void)
{
    /* Configure SCK as output, DOUT as input */
    nrf_gpio_cfg_output(SB_GPIO_HX711_SCK);
    nrf_gpio_cfg_input(SB_GPIO_HX711_DOUT, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_clear(SB_GPIO_HX711_SCK);

    /* Power up: SCK low for >100μs resets HX711 from power-down */
    nrf_delay_us(200);

    /* Set gain (send pulses to channel/gain select) */
    /* 1 pulse = Channel A, gain 128 */
    /* 2 pulses = Channel B, gain 32 */
    /* 3 pulses = Channel A, gain 64 */
    for (uint8_t i = 0; i < g_hx711_gain; i++) {
        nrf_gpio_pin_set(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
        nrf_gpio_pin_clear(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
    }
}

static bool hx711_is_ready(void)
{
    return nrf_gpio_pin_read(SB_GPIO_HX711_DOUT) == 0;
}

static int32_t hx711_read_raw(void)
{
    /* Wait for data ready (DOUT goes low) */
    uint32_t timeout = 100000;
    while (!hx711_is_ready() && timeout-- > 0) {
        nrf_delay_us(10);
    }
    if (timeout == 0) return 0;  /* timeout */

    /* Read 24 bits */
    int32_t value = 0;
    for (uint8_t i = 0; i < 24; i++) {
        nrf_gpio_pin_set(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
        value = (value << 1) | nrf_gpio_pin_read(SB_GPIO_HX711_DOUT);
        nrf_gpio_pin_clear(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
    }

    /* Set gain for next reading (1-3 pulses) */
    for (uint8_t i = 0; i < g_hx711_gain; i++) {
        nrf_gpio_pin_set(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
        nrf_gpio_pin_clear(SB_GPIO_HX711_SCK);
        nrf_delay_us(1);
    }

    /* Sign extend 24-bit to 32-bit */
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    return value;
}

static void hx711_tare(void)
{
    /* Take 10 readings and average for zero offset */
    int64_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += hx711_read_raw();
        nrf_delay_ms(12);  /* ~80 Hz max sample rate */
    }
    g_hx711_offset = (int32_t)(sum / 10);
    NRF_LOG_INFO("HX711 tare offset: %ld", g_hx711_offset);
}

static float hx711_get_force_kg(void)
{
    int32_t raw = hx711_read_raw();
    int32_t net = raw - g_hx711_offset;
    return (float)net * g_hx711_scale;
}

/* === LSM6DSO IMU (shared SPI with HX711 on different pins) === */
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);
static volatile bool g_spi_xfer_done = true;

static void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
    g_spi_xfer_done = true;
}

static void spi_init(void)
{
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.ss_pin   = NRF_DRV_SPI_PIN_NOT_USED;
    spi_cfg.mosi_pin = SB_GPIO_SPI_MOSI;
    spi_cfg.miso_pin = SB_GPIO_SPI_MISO;
    spi_cfg.sck_pin  = SB_GPIO_SPI_SCK;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_8M;
    spi_cfg.mode = NRF_DRV_SPI_MODE_3;
    APP_ERROR_CHECK(nrf_drv_spi_init(&g_spi, &spi_cfg, spi_event_handler, NULL));
    nrf_gpio_cfg_output(SB_GPIO_IMU_CS);
    nrf_gpio_pin_set(SB_GPIO_IMU_CS);
}

static void spi_write_reg(uint8_t reg, uint8_t val)
{
    nrf_gpio_pin_clear(SB_GPIO_IMU_CS);
    uint8_t tx[2] = { reg & 0x7F, val };
    g_spi_xfer_done = false;
    nrf_drv_spi_transfer(&g_spi, tx, 2, NULL, 0);
    while (!g_spi_xfer_done);
    nrf_gpio_pin_set(SB_GPIO_IMU_CS);
}

static void spi_read_burst(uint8_t reg, uint8_t *buf, size_t len)
{
    nrf_gpio_pin_clear(SB_GPIO_IMU_CS);
    buf[0] = reg | 0x80;
    g_spi_xfer_done = false;
    nrf_drv_spi_transfer(&g_spi, buf, len + 1, buf, len + 1);
    while (!g_spi_xfer_done);
    nrf_gpio_pin_set(SB_GPIO_IMU_CS);
    memmove(buf, buf + 1, len);
}

static void imu_init(void)
{
    spi_write_reg(0x12, 0x44); /* CTRL3_C: BDU=1, IF_INC=1 */
    nrf_delay_ms(10);
    spi_write_reg(0x10, 0x4C); /* CTRL1_XL: 104Hz, ±8g */
    spi_write_reg(0x11, 0x4C); /* CTRL2_G: 104Hz, ±2000dps */
}

static void imu_read(int16_t *accel, int16_t *gyro)
{
    uint8_t buf[12];
    spi_read_burst(0x22, buf, 6);
    gyro[0] = (int16_t)((buf[1] << 8) | buf[0]);
    gyro[1] = (int16_t)((buf[3] << 8) | buf[2]);
    gyro[2] = (int16_t)((buf[5] << 8) | buf[4]);
    spi_read_burst(0x28, buf, 6);
    accel[0] = (int16_t)((buf[1] << 8) | buf[0]);
    accel[1] = (int16_t)((buf[3] << 8) | buf[2]);
    accel[2] = (int16_t)((buf[5] << 8) | buf[4]);
}

/* === BLE GATT Service === */
static ble_nus_t g_nus;
static uint16_t g_conn_handle = BLE_CONN_HANDLE_INVALID;
static bool g_ble_connected = false;

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            g_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            g_ble_connected = true;
            NRF_LOG_INFO("BLE connected");
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            g_conn_handle = BLE_CONN_HANDLE_INVALID;
            g_ble_connected = false;
            NRF_LOG_INFO("BLE disconnected, advertising...");
            break;
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
            {
                ble_gap_phys_t phys = {
                    .rx_phys = BLE_GAP_PHY_2MBPS,
                    .tx_phys = BLE_GAP_PHY_2MBPS,
                };
                sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            }
            break;
    }
}

static void ble_init(void)
{
    /* BLE stack init, advertising, NUS service */
}

/* === MAX17048 Fuel Gauge === */
static uint8_t max17048_read_reg(uint8_t reg)
{
    /* I²C read from MAX17048 */
    uint8_t val = 0;
    /* nrf_drv_twi_tx + nrf_drv_twi_rx */
    return val;
}

static uint8_t get_battery_pct(void)
{
    /* Read SOC register from MAX17048 */
    uint8_t soc = max17048_read_reg(0x10);
    return soc;
}

/* === Force Sampling Task (50 Hz) === */
static void force_task(void *arg)
{
    rs_force_sample_t force_pkt;
    int16_t accel[3], gyro[3];

    /* Tare on startup */
    hx711_tare();

    while (1) {
        /* Read force at 50 Hz (HX711 max ~80 Hz) */
        float force_kg = hx711_get_force_kg();
        int32_t force_mg = (int32_t)(force_kg * 1000.0f);  /* kg to mg-force */
        force_pkt.force_mg = force_mg;

        /* Read IMU for tempo/orientation */
        imu_read(accel, gyro);

        /* Stream via BLE if connected */
        if (g_ble_connected) {
            /* Packet: [force(4) | accel(6) | gyro(6)] = 16 bytes per sample
             * At 50 Hz: 800 bytes/s — well within BLE capacity
             */
            uint8_t ble_buf[16];
            memcpy(ble_buf, &force_pkt, 4);
            memcpy(ble_buf + 4, accel, 6);
            memcpy(ble_buf + 10, gyro, 6);
            /* ble_nus_data_send(&g_nus, ble_buf, &len, g_conn_handle); */
        }

        /* Status LED: green if force > 0, off if idle */
        if (force_kg > 0.5f) {
            /* nrf_gpio_pin_set(SB_GPIO_LED); */
        } else {
            /* nrf_gpio_pin_clear(SB_GPIO_LED); */
        }

        nrf_delay_ms(20);  /* 50 Hz */
    }
}

/* === Power Management Task === */
static void power_task(void *arg)
{
    while (1) {
        uint8_t batt = get_battery_pct();
        if (batt < 20) {
            NRF_LOG_WARNING("Low battery: %d%%", batt);
            /* Send low battery alert via BLE */
        }

        /* Check USB-C charging status */
        /* If charging: LED red, else: LED off */

        nrf_delay_ms(60000);  /* check every 60s */
    }
}

/* === Main === */
int main(void)
{
    bsp_board_init(BSP_INIT_LEDS);
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("RehabSync Smart Band starting...");

    /* Initialize SPI for IMU */
    spi_init();
    imu_init();

    /* Initialize HX711 */
    hx711_init();

    /* Initialize BLE */
    ble_init();

    /* Start force sampling */
    force_task(NULL);

    while (1) {
        __WFI();
        NRF_LOG_FLUSH();
    }
}
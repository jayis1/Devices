/*
 * GuideSync — Nav Beacon Firmware
 * nRF52840, bare-metal (nRF SDK / nRF Connect SDK)
 *
 * The Nav Beacon is a battery-powered BLE advertising node (CR2032,
 * 12–18 month life). It broadcasts a BLE advertisement containing
 * a unique UUID prefix (0x47, 0x53, 0xBE, 0xAC) + unique node ID,
 * enabling the glasses and haptic band to RSSI-fingerprint their
 * indoor position via NavNet.
 *
 * Configuration mode: hold a magnet near the reed switch to enter
 * config mode (BLE connectable, landmark name + coordinates set
 * via mobile app).
 *
 * Build: west build -b nrf52840dongle_nrf52840
 */
#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "ble_advdata.h"
#include "nrf_drv_gpiote.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"

#include "../common/config.h"

#define LOG(...) NRF_LOG_INFO(__VA_ARGS__)

/* === Beacon Configuration (stored in flash) === */
typedef struct {
    uint16_t beacon_id;        /* Unique 16-bit ID */
    uint8_t  uuid_prefix[4];   /* 0x47, 0x53, 0xBE, 0xAC */
    uint8_t  tx_power_dbm;     /* TX power (0 dBm default) */
    uint16_t adv_interval_ms;  /* Advertising interval (500 ms default) */
    uint8_t  battery_v;        /* Last battery reading (x0.01V) */
    uint8_t  config_mode;      /* 1 = in configuration mode */
} beacon_config_t;

static beacon_config_t g_config = {
    .beacon_id = 0x0001,
    .uuid_prefix = {0x47, 0x53, 0xBE, 0xAC},
    .tx_power_dbm = 0,
    .adv_interval_ms = GS_BEACON_ADV_INTERVAL_MS,
    .battery_v = 300, /* 3.00V CR2032 nominal */
    .config_mode = 0,
};

/* === BLE Advertising === */
static ble_advdata_t g_advdata;
static ble_advdata_manuf_data_t g_manuf_data;
static uint8_t g_adv_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static ble_gap_adv_data_t g_adv_set = {
    .adv_data.p_data = g_adv_data,
    .adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX,
};

/* Build advertisement payload: manufacturer-specific data with beacon UUID */
static void build_advertisement(void)
{
    memset(&g_advdata, 0, sizeof(g_advdata));
    memset(&g_manuf_data, 0, sizeof(g_manuf_data));

    /* Manufacturer-specific data: prefix + beacon_id + battery */
    uint8_t manuf_payload[7];
    manuf_payload[0] = g_config.uuid_prefix[0]; /* 0x47 */
    manuf_payload[1] = g_config.uuid_prefix[1]; /* 0x53 */
    manuf_payload[2] = g_config.uuid_prefix[2]; /* 0xBE */
    manuf_payload[3] = g_config.uuid_prefix[3]; /* 0xAC */
    manuf_payload[4] = (uint8_t)(g_config.beacon_id & 0xFF);
    manuf_payload[5] = (uint8_t)(g_config.beacon_id >> 8);
    manuf_payload[6] = g_config.battery_v;

    g_manuf_data.company_identifier = 0x0059; /* Nordic Semiconductor */
    g_manuf_data.data.p_data = manuf_payload;
    g_manuf_data.data.size = sizeof(manuf_payload);

    g_advdata.name_type = BLE_ADVDATA_NO_NAME;
    g_advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    g_advdata.p_manuf_specific_data = &g_manuf_data;

    /* Encode */
    uint16_t len = sizeof(g_adv_data);
    ble_advdata_encode(&g_advdata, g_adv_data, &len);
    g_adv_set.adv_data.len = len;
}

/* Start BLE advertising */
static void advertising_start(void)
{
    build_advertisement();

    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr = NULL;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = (g_config.adv_interval_ms * 8) / 5; /* 0.625 ms units */
    adv_params.duration = 0; /* Never timeout */

    sd_ble_gap_adv_set_configure(&g_adv_set, &g_adv_set.adv_data, &adv_params);
    sd_ble_gap_adv_start(g_adv_set.handle, 1);
    LOG("Advertising started (ID=0x%04X, interval=%d ms)",
        g_config.beacon_id, g_config.adv_interval_ms);
}

/* === Reed Switch (config mode) === */
static void reed_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    if (nrf_drv_gpiote_in_is_set(pin)) {
        /* Magnet removed — exit config mode */
        g_config.config_mode = 0;
        LOG("Config mode OFF — resuming advertising");
        advertising_start();
    } else {
        /* Magnet present — enter config mode (connectable for app) */
        g_config.config_mode = 1;
        sd_ble_gap_adv_stop(g_adv_set.handle);
        LOG("Config mode ON — connectable for setup");
        /* Production: switch to connectable advertising with GATT service
         * for landmark name + coordinate configuration */
    }
}

static void reed_init(void)
{
    nrf_drv_gpiote_init();
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    config.pull = NRF_GPIO_PIN_PULLUP;
    nrf_drv_gpiote_in_init(BEACON_GPIO_REED, &config, reed_handler);
    nrf_drv_gpiote_in_event_enable(BEACON_GPIO_REED, true);
}

/* === Battery Monitor === */
static uint8_t read_battery_v(void)
{
    /* Production: SAADC on BEACON_GPIO_VBAT, VDD/3 divider
     * CR2032: 3.0V nominal, 2.7V low */
    /* Stub: simulate slow discharge */
    static uint8_t v = 300;
    /* In production: actual ADC read */
    return v;
}

/* === LED Status === */
static void led_init(void)
{
    nrf_gpio_cfg_output(BEACON_GPIO_LED);
    nrf_gpio_pin_write(BEACON_GPIO_LED, 0);
}

static void led_blink(void)
{
    nrf_gpio_pin_write(BEACON_GPIO_LED, 1);
    nrf_delay_ms(10);
    nrf_gpio_pin_write(BEACON_GPIO_LED, 0);
}

/* === Battery Check Timer ===
 * Check battery every 1 hour, update advertisement payload
 */
APP_TIMER_DEF(m_battery_timer);

static void battery_timer_handler(void *p_context)
{
    g_config.battery_v = read_battery_v();

    /* Low battery: blink LED rapidly */
    if (g_config.battery_v < BAT_CR2032_LOW_MV) {
        for (int i = 0; i < 5; i++) {
            led_blink();
            nrf_delay_ms(100);
        }
        LOG("Low battery: %.2fV", g_config.battery_v / 100.0);
    }

    /* Update advertisement with new battery reading */
    if (!g_config.config_mode) {
        build_advertisement();
    }
}

/* === Main === */
int main(void)
{
    LOG("GuideSync Nav Beacon starting (ID=0x%04X)...", g_config.beacon_id);

    /* SoftDevice enable */
    ble_enable_params_t ble_params;
    memset(&ble_params, 0, sizeof(ble_params));
    ble_params.common_enable_params.vs_uuid_count = 1;
    ble_params.gap_enable_params.role = BLE_GAP_ROLE_ADV;
    ble_params.gap_enable_params.mtu = 23;

    sd_enable();
    sd_ble_enable(&ble_params);

    /* Init peripherals */
    led_init();
    reed_init();
    app_timer_init();
    app_timer_create(&m_battery_timer, APP_TIMER_MODE_REPEATED, battery_timer_handler);

    /* Start advertising */
    advertising_start();

    /* Start battery timer (3600 second interval) */
    app_timer_start(m_battery_timer, APP_TIMER_TICKS(3600000), NULL);

    /* Initial battery read */
    g_config.battery_v = read_battery_v();

    LOG("Nav Beacon ready. Battery: %.2fV", g_config.battery_v / 100.0);

    /* Main loop: sleep between advertising (System ON sleep) */
    while (true) {
        nrf_pwr_mgmt_run();
        /* Wakes on timer or reed switch interrupt */
    }
}

/* Power management handler */
void nrf_pwr_mgmt_run(void)
{
    __WFE();
    __SEV();
    __WFE();
}
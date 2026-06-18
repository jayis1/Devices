/*
 * comfort_tag_main.c — ThermoGrid Comfort Tag (nRF52840)
 *
 * A wearable that reads skin temperature, heart rate, activity, and local
 * humidity to learn your personal thermal comfort profile.
 *
 * Responsibilities:
 * - Measure skin temperature (MAX30208, clinical-grade ±0.1°C)
 * - Measure ambient air temperature near body (TMP117, ±0.1°C)
 * - Measure heart rate + HRV (MAX30101 PPG)
 * - Classify activity level (LSM6DSO 6-axis IMU: sedentary/light/moderate/vigorous/sleeping)
 * - Measure local humidity (SHT40)
 * - Predict comfort score (-3 cold .. 0 neutral .. +3 hot) via on-device model
 * - "I'm cold" / "I'm hot" button → comfort vote → sent to hub → trains personal model
 * - BLE peripheral: advertises comfort data, connects to hub or phone
 * - Ultra-low-power: 8-12 months on CR2032
 *
 * nRF52840: Cortex-M4F, BLE 5.0, ultra-low-power
 */

#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_gpiote.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_advdata.h"
#include "nrf_ble_gatt.h"
#include "mesh_protocol.h"

static const char *TAG = "COMFORT_TAG";

/* ---- Pin Definitions (nRF52840) ---- */
#define PIN_MAX30208_SDA   26   /* TWI0 */
#define PIN_MAX30208_SCL   27
#define PIN_TMP117_SDA     26   /* shared TWI0 */
#define PIN_TMP117_SCL     27
#define PIN_SHT40_SDA       26   /* shared TWI0 */
#define PIN_SHT40_SCL       27
#define PIN_MAX30101_SDA   24   /* TWI1 (separate bus for timing) */
#define PIN_MAX30101_SCL   25
#define PIN_LSM6DSO_SDA    24   /* shared TWI1 */
#define PIN_LSM6DSO_SCL    25

#define PIN_VOTE_COLD      13   /* button: "I'm cold" */
#define PIN_VOTE_HOT       14   /* button: "I'm hot" */
#define PIN_LED            15   /* status LED */
#define PIN_BATT_SENSE     30   /* SAADC for battery voltage */

#define PIN_MAX30101_INT   11   /* PPG interrupt */
#define PIN_LSM6DSO_INT1   12   /* IMU interrupt (activity change) */

/* ---- Node identity ---- */
#define MY_PERSON_ID    0x80   /* person 0 (0x80-0x8F range) */
#define MY_NODE_ID      0x80

/* ---- TWI (I2C) instances ---- */
static const nrf_drv_twi_t twi0 = NRF_DRV_TWI_INSTANCE(0);
static const nrf_drv_twi_t twi1 = NRF_DRV_TWI_INSTANCE(1);

/* ---- Sensor Read Functions ---- */

/* MAX30208: I2C addr 0x50, clinical-grade skin temp ±0.1°C */
static float max30208_read(void)
{
    /* In production:
     * 1. I2C write 0x01 (temperature register) — or just read
     * 2. Read 2 bytes: MSB, LSB
     * 3. Temp = raw * 0.00390625 (1/256) °C
     *    (16-bit, resolution 0.0039°C)
     *
     * Stub: return typical wrist skin temp */
    return 31.5f;
}

/* TMP117: I2C addr 0x48, air temp near body ±0.1°C */
static float tmp117_read(void)
{
    /* In production:
     * 1. I2C read 2 bytes from register 0x00
     * 2. Temp = raw * 0.0078125 °C (resolution 1/128 °C)
     *
     * Stub: */
    return 24.0f;
}

/* SHT40: I2C addr 0x44, humidity near skin */
static float sht40_read_humidity(float *temp_out)
{
    /* In production:
     * 1. I2C send command 0xFD (high repeatability, clock stretching)
     * 2. Read 6 bytes: T MSB, T LSB, T CRC, H MSB, H LSB, H CRC
     * 3. T = -45 + 175 * (T_raw / 65535)
     * 4. RH = 125 * (H_raw / 65535) - 6
     *
     * Stub: */
    *temp_out = 24.0f;
    return 50.0f;
}

/* MAX30101: I2C addr 0x57, PPG heart rate + HRV */
static void max30101_read(uint8_t *hr_bpm, uint8_t *hrv_ms)
{
    /* In production:
     * 1. Configure: RED+IR LEDs, 100Hz sample, 4110us pulse width, 69 ADC range
     * 2. Read FIFO (17 samples × 2 channels × 3 bytes)
     * 3. Process IR signal: detect peaks → HR
     * 4. Compute RR intervals → HRV (RMSSD)
     *
     * Stub: */
    *hr_bpm = 72;
    *hrv_ms = 45;
}

/* LSM6DSO: I2C addr 0x6A, 6-axis IMU for activity classification */
static uint8_t lsm6dso_read_activity(uint8_t *conf)
{
    /* In production:
     * 1. Read accelerometer (3 axes, ±4g, 12.5Hz)
     * 2. Read gyro (3 axes, 500dps)
     * 3. Compute features: variance, energy, dominant frequency
     * 4. Classify: sedentary/light/moderate/vigorous/sleeping
     *    - Sedentary: low variance, low energy (<0.01 g²)
     *    - Light: walking pattern, ~1-2 Hz, moderate variance
     *    - Moderate: brisk walk, 2-3 Hz, higher variance
     *    - Vigorous: running, >3 Hz, high variance
     *    - Sleeping: very low variance, specific orientation
     *
     * Stub: */
    *conf = 200;
    return ACT_SEDENTARY;
}

/* ---- Comfort Prediction (on-device lightweight model) ---- */

/* In production: TFLite Micro or a small decision tree
 * Model trained on personal vote data:
 *   comfort = f(skin_temp, air_temp, humidity, HR, HRV, activity)
 *
 * Features that matter most:
 *   - skin_temp < 28°C → likely cold
 *   - skin_temp > 34°C → likely hot
 *   - high HR + high activity → body generating heat, may feel warm
 *   - low HR + sedentary → less metabolic heat, may feel cool
 *   - high humidity + warm skin → sweat evaporation reduced, feel hotter
 *
 * This is personalized: cold-tolerant people have a shifted baseline.
 * The model learns your personal offset from the population PMV model.
 */
static int8_t predict_comfort(float skin_temp, float air_temp, float humidity,
                               uint8_t hr, uint8_t activity)
{
    /* Stub: simple heuristic based on skin temperature */
    float comfort = 0.0f;

    /* Baseline from skin temp */
    if (skin_temp < 28.0f) comfort -= 2.0f;
    else if (skin_temp < 30.0f) comfort -= 1.0f;
    else if (skin_temp < 32.0f) comfort -= 0.0f;
    else if (skin_temp < 34.0f) comfort += 0.0f;
    else if (skin_temp < 36.0f) comfort += 1.0f;
    else comfort += 2.0f;

    /* Adjust for activity (more active = more metabolic heat) */
    if (activity >= ACT_MODERATE) comfort += 0.5f;
    if (activity == ACT_SLEEPING) comfort -= 0.5f;

    /* Adjust for humidity (high humidity reduces sweat evaporation) */
    if (humidity > 60.0f && skin_temp > 32.0f) comfort += 0.5f;

    /* Clamp to [-3, +3] */
    if (comfort < -3.0f) comfort = -3.0f;
    if (comfort > 3.0f) comfort = 3.0f;

    return (int8_t)comfort;
}

/* ---- Comfort data payload ---- */

static comfort_data_payload_t latest_data;
static uint16_t seq_num = 0;
static uint8_t vote_pending = 0; /* 0=none, 1=cold, 2=hot */

/* ---- Build comfort data ---- */

static void build_comfort_data(void)
{
    float skin_temp = max30208_read();
    float air_temp = tmp117_read();
    float sht40_temp;
    float humidity = sht40_read_humidity(&sht40_temp);
    uint8_t hr_bpm, hrv_ms;
    max30101_read(&hr_bpm, &hrv_ms);
    uint8_t act_conf;
    uint8_t activity = lsm6dso_read_activity(&act_conf);

    int8_t comfort_score = predict_comfort(skin_temp, air_temp, humidity,
                                             hr_bpm, activity);

    memset(&latest_data, 0, sizeof(latest_data));
    latest_data.skin_temp_cx100 = (int16_t)(skin_temp * 100);
    latest_data.air_temp_cx100 = (int16_t)(air_temp * 100);
    latest_data.humidity_centi = (uint16_t)(humidity * 100);
    latest_data.hr_bpm = hr_bpm;
    latest_data.hrv_ms = hrv_ms;
    latest_data.activity = activity;
    latest_data.activity_conf = act_conf;
    latest_data.comfort_score = comfort_score;
    latest_data.comfort_conf = 180; /* 70% confidence */
    latest_data.person_id = MY_PERSON_ID;
    latest_data.vote_pending = vote_pending;
    latest_data.battery_pct = read_battery_pct();
    latest_data.signal_rssi = -60;
    latest_data.seq_num = seq_num++;

    printf("[COMFORT] skin=%.1f°C air=%.1f°C H=%.0f%% HR=%d HRV=%d act=%d comfort=%d vote=%d bat=%d\n",
           skin_temp, air_temp, humidity, hr_bpm, hrv_ms,
           activity, comfort_score, vote_pending, latest_data.battery_pct);
}

/* ---- Battery reading ---- */

static uint8_t read_battery_pct(void)
{
    /* In production: SAADC read battery voltage
     * CR2032: 3.0V fresh, 2.2V dead
     * Map 3.0V→100%, 2.2V→0% */
    return 85;
}

/* ---- BLE: comfort data service ---- */

/* Custom GATT service: 0xTG00 (ThermoGrid comfort)
 * Characteristics:
 *   - Comfort Data (read + notify): latest comfort_data_payload_t
 *   - Vote (write): trigger a vote
 *   - Person ID (read): which person this tag belongs to
 */

/* BLE advertising: include person_id + comfort_score in manufacturer data */
static void ble_advertise(void)
{
    /* In production:
     * 1. Set advertising data: name "TG-Tag-XX", manufacturer data
     *    [person_id, comfort_score, battery_pct]
     * 2. Set advertising interval: 2s (connected) / 5s (sleep)
     * 3. Start advertising
     */
}

static void ble_send_comfort_data(void)
{
    /* In production: if connected, send notification with comfort_data_payload_t
     * If not connected, the hub can scan for advertising packets
     * and read comfort_score from manufacturer data */
}

/* ---- Vote button handling ---- */

static void vote_cold_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    vote_pending = 1; /* "I'm cold" */
    nrf_gpio_pin_set(PIN_LED);
    nrf_delay_ms(100);
    nrf_gpio_pin_clear(PIN_LED);
    printf("[VOTE] I'm COLD pressed\n");
}

static void vote_hot_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    vote_pending = 2; /* "I'm hot" */
    nrf_gpio_pin_set(PIN_LED);
    nrf_delay_ms(100);
    nrf_gpio_pin_clear(PIN_LED);
    printf("[VOTE] I'm HOT pressed\n");
}

/* ---- Build BLE packet for hub ---- */

static void send_to_hub_ble(void)
{
    /* In production: if connected to hub via BLE,
     * send comfort_data_payload_t as GATT notification
     *
     * If not connected, the comfort data is in the advertising
     * manufacturer data (compressed: person_id, comfort_score, battery)
     *
     * Hub scans for TG-Tag advertisements every 30s */

    /* Also build mesh packet (in case hub relays via Sub-GHz) */
    mesh_packet_t pkt;
    uint16_t pkt_len = mesh_build_packet(
        MY_NODE_ID, NODE_ID_HUB, PKT_COMFORT_DATA,
        (uint8_t *)&latest_data, sizeof(latest_data), &pkt);

    /* In production: send via BLE GATT write to hub service */
    printf("[BLE] Sent comfort data (%d bytes) to hub\n", pkt_len);
}

static void send_vote_to_hub(void)
{
    comfort_vote_payload_t vote;
    vote.person_id = MY_PERSON_ID;
    vote.vote = (vote_pending == 1) ? COMF_COOL : COMF_WARM;
    vote.skin_temp_cx100 = latest_data.skin_temp_cx100;
    vote.activity = latest_data.activity;
    vote.room_id = 0xFF; /* hub determines room from BLE RSSI */
    vote.reserved[0] = 0;

    mesh_packet_t pkt;
    uint16_t pkt_len = mesh_build_packet(
        MY_NODE_ID, NODE_ID_HUB, PKT_COMFORT_VOTE,
        (uint8_t *)&vote, sizeof(vote), &pkt);

    printf("[VOTE] Sending vote to hub: %s (skin=%.1f°C)\n",
           vote_pending == 1 ? "COLD" : "HOT",
           vote.skin_temp_cx100 / 100.0f);

    vote_pending = 0; /* clear after sending */
}

/* ---- Deep sleep / power management ---- */

static void enter_sleep(uint32_t seconds)
{
    /* In production:
     * 1. Stop all sensors (power gate via MOSFET or I2C standby)
     * 2. Configure RTC wakeup after 'seconds'
     * 3. Keep GPIO interrupt for vote buttons (wakeup on press)
     * 4. Enter SystemOFF mode (<3µA)
     *    Or: connect MAX30101 in proximity mode (detects wrist presence)
     * 5. On wakeup: re-init sensors, measure, send, sleep again
     */
    /* nrf_pwr_mgmt_run(); */
}

/* ---- Main ---- */

int main(void)
{
    /* Initialize nRF52840 */
    nrf_drv_twi_config_t twi_config = {
        .scl = PIN_MAX30208_SCL,
        .sda = PIN_MAX30208_SDA,
        .frequency = NRF_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
    };
    nrf_drv_twi_init(&twi0, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&twi0);

    twi_config.scl = PIN_MAX30101_SCL;
    twi_config.sda = PIN_MAX30101_SDA;
    nrf_drv_twi_init(&twi1, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&twi1);

    /* GPIO for vote buttons */
    nrf_drv_gpiote_init();
    nrf_drv_gpiote_in_config_t btn_config = {
        .sense = NRF_GPIOTE_POLARITY_HITOLO,
        .pull = NRF_GPIO_PIN_PULLUP,
        .is_watcher = false,
    };
    nrf_drv_gpiote_in_init(PIN_VOTE_COLD, &btn_config, vote_cold_handler);
    nrf_drv_gpiote_in_init(PIN_VOTE_HOT, &btn_config, vote_hot_handler);
    nrf_drv_gpiote_in_event_enable(PIN_VOTE_COLD, true);
    nrf_drv_gpiote_in_event_enable(PIN_VOTE_HOT, true);

    /* LED */
    nrf_gpio_cfg_output(PIN_LED);
    nrf_gpio_pin_clear(PIN_LED);

    /* SAADC for battery */
    /* nrf_drv_saadc_init(...); */

    /* BLE init */
    /* ble_stack_init(); */
    /* ble_advertise(); */

    printf("\n=== ThermoGrid Comfort Tag v1.0 (person=0x%02X) ===\n",
           MY_PERSON_ID);

    uint32_t measurement_interval_s = 30; /* 30s default */
    bool connected = false;

    while (1) {
        /* Measure all sensors */
        build_comfort_data();

        /* Send to hub (BLE) */
        send_to_hub_ble();

        /* If vote button was pressed, send vote immediately */
        if (vote_pending != 0) {
            send_vote_to_hub();
        }

        /* Adjust measurement interval based on activity and battery */
        if (latest_data.battery_pct < 20) {
            measurement_interval_s = 300; /* 5 min if low battery */
        } else if (latest_data.activity >= ACT_MODERATE) {
            measurement_interval_s = 15; /* 15s if active (more dynamic) */
        } else {
            measurement_interval_s = 30; /* 30s normal */
        }

        /* Blink LED briefly to show alive */
        nrf_gpio_pin_set(PIN_LED);
        nrf_delay_ms(10);
        nrf_gpio_pin_clear(PIN_LED);

        /* Sleep until next measurement or vote button press */
        enter_sleep(measurement_interval_s);
    }
}
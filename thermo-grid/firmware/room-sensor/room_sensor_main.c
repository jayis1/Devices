/*
 * room_sensor_main.c — ThermoGrid Room Sensor Node (STM32WL55JC)
 *
 * Responsibilities:
 * - Measure air temperature, humidity (SHT45 — ±0.1°C reference)
 * - Measure mean radiant temperature (MLX90640 16×12 thermal IR array)
 * - Measure air velocity / draft (SDP810 differential pressure)
 * - Measure barometric pressure (BMP390)
 * - Detect occupancy (HLK-LD2410B mmWave + AM612 PIR)
 * - Measure ambient light (ALS-PT19)
 * - Optional CO2 (SCD41)
 * - Detect open windows (rapid temp drop + air velocity + humidity change)
 * - Estimate solar gain (light + MRT of sunlit surface)
 * - Report to hub over LoRa every 30s (or immediately on WINDOW_OPEN)
 * - Ultra-low-power: deep sleep <8µA, solar + AA battery
 *
 * STM32WL55JC: Cortex-M4 + integrated Sub-GHz LoRa radio (no external radio chip)
 */

#include <string.h>
#include <math.h>
#include "stm32wl5xx_hal.h"
#include "mesh_protocol.h"

/* ---- Pin Definitions (STM32WL55JC) ---- */
/* Sub-GHz radio is internal (Semtech SX126x IP in the STM32WL) */

/* I2C bus for sensors */
#define I2C_INSTANCE    hi2c2
/* SHT45, BMP390, SCD41 on I2C2: PB10=SDA, PB11=SCL */

/* MLX90640 on a separate I2C (I2C1) for timing isolation */
/* PA9=SDA, PA10=SCL (or use same bus with clock stretching) */

/* PIR interrupt pin */
#define PIN_PIR_IRQ      PA0   /* AM612 motion output (edge wakeup) */

/* mmWave UART */
#define MMWAVE_UART      huart1
/* PA2=TX, PA3=RX for HLK-LD2410B */

/* Light sensor (ALS-PT19) — ADC */
#define PIN_LIGHT_ADC    PA1   /* ALS-PT19 analog output */

/* Battery voltage divider — ADC */
#define PIN_BATT_ADC     PA4

/* Solar panel voltage — ADC */
#define PIN_SOLAR_ADC    PA5

/* User calibration button */
#define PIN_USER_BTN     PB0   /* active low */

/* ---- Sensor calibration ---- */
typedef struct {
    float temp_offset;     /* SHT45 temp offset °C */
    float humidity_offset;  /* SHT45 humidity offset % */
    float mrt_offset;       /* MLX90640 offset °C */
    float air_vel_scale;   /* SDP810 scale factor */
    float light_scale;     /* ALS-PT19 scale */
} sensor_calibration_t;

static sensor_calibration_t cal = {
    .temp_offset = 0.0f,
    .humidity_offset = 0.0f,
    .mrt_offset = 0.0f,
    .air_vel_scale = 1.0f,
    .light_scale = 1.0f,
};

/* ---- Node identity ---- */
#define MY_NODE_ID      0x10   /* configured at enrollment */
#define MY_ZONE_ID       0     /* which zone/room */

/* ---- Measurement state ---- */
static sensor_data_payload_t latest_data;
static uint16_t seq_num = 0;

/* Window detection state */
static float prev_temp = 0.0f;
static float prev_humidity = 0.0f;
static float prev_air_vel = 0.0f;
static absolute_time_t prev_measure_time;
static uint8_t window_open_confirmations = 0;

/* ---- Sensor Read Functions ---- */

/* SHT45: I2C address 0x44, temp ±0.1°C, humidity ±1.5%RH */
static void sht45_read(float *temp, float *humidity)
{
    /* In production: I2C send command 0xFD (clock stretching single shot),
     * read 6 bytes: temp MSB, temp LSB, CRC, hum MSB, hum LSB, CRC
     * T = -45 + 175 * (raw / 65535)
     * RH = 100 * (raw / 65535)
     */
    *temp = 21.5f + cal.temp_offset;
    *humidity = 45.0f + cal.humidity_offset;
}

/* MLX90640: I2C address 0x33, 16×12 thermal IR array (192 pixels) */
static void mlx90640_read(float *mrt, float *max_surface_temp, uint8_t *solar_gain_w)
{
    /* In production:
     * 1. Read 832 bytes of EEPROM for calibration params
     * 2. Read 832 bytes of RAM (2 sub-pages)
     * 3. Apply calibration: T = (alpha * (raw - offset) / k) ^ 0.25 - 273.15
     * 4. Compute mean of all 192 pixels = MRT
     * 5. Find max pixel = warmest surface (window/wall/floor)
     * 6. If sunlit wall visible: estimate solar gain from surface temp delta
     *
     * Stub: return typical room values */
    *mrt = 20.0f + cal.mrt_offset;
    *max_surface_temp = 22.0f;
    *solar_gain_w = 0;
}

/* SDP810: I2C address 0x25, differential pressure → air velocity */
static void sdp810_read(float *air_vel_cms)
{
    /* In production: I2C read 3 bytes (MSB, LSB, CRC)
     * Pressure Pa = (raw / 32768) * range (e.g., 125 Pa)
     * Air velocity ≈ sqrt(2 * dP / rho), rho = 1.2 kg/m³
     * Convert m/s to cm/s
     */
    *air_vel_cms = 5.0f * cal.air_vel_scale; /* 5 cm/s baseline */
}

/* BMP390: I2C address 0x77, barometric pressure */
static void bmp390_read(float *pressure_hpa, float *temp)
{
    /* In production: read calibration from EEPROM, read raw pressure/temp,
     * apply compensation formulas from datasheet */
    *pressure_hpa = 1013.25f;
    *temp = 21.0f;
}

/* ALS-PT19: analog light sensor */
static uint16_t als_read_lux(void)
{
    /* In production: ADC read, convert using scale factor
     * Lux = adc_value * cal.light_scale * 10 (approx) */
    return 250; /* 250 lux baseline (indoor) */
}

/* SCD41: I2C address 0x62, photoacoustic CO2 */
static uint16_t scd41_read_co2(void)
{
    /* In production: send 0x219D command, wait 5ms, read 9 bytes
     * CO2 = (MSB << 8) | LSB
     * Only if SCD41 present (optional sensor) */
    return 0; /* 0 = no CO2 sensor */
}

/* ---- Occupancy detection ---- */

static void occupancy_detect(uint8_t *occ_state, uint8_t *occ_conf)
{
    /* In production:
     * 1. mmWave (HLK-LD2410B) via UART: read engineering mode data
     *    - Distinguishes static (still person) vs dynamic (moving person)
     *    - Range gates: 0-8, distance resolution 0.75m
     *    - Can detect breathing (micro-motion) even when still
     * 2. PIR (AM612): fast motion trigger, wakes from deep sleep
     * 3. Combine: mmWave for static+dynamic, PIR for fast wakeup
     *
     * States: 0=empty, 1=person, 2=pet (small target, low gate), 3=multi
     */
    *occ_state = OCC_EMPTY;
    *occ_conf = 0;
}

/* ---- Open window detection ---- */

static int8_t detect_window_open(float current_temp, float current_humidity,
                                  float current_air_vel)
{
    /* Window open signature:
     * 1. Temperature drops >1°C in <60 seconds
     * 2. Air velocity spike >50 cm/s (draft from window)
     * 3. Humidity change >5% in <60s (outdoor air different humidity)
     * 4. Sustained: condition persists for 2+ confirmations
     */

    float temp_drop = prev_temp - current_temp;
    float hum_change = current_humidity - prev_humidity;
    float vel_spike = current_air_vel - prev_air_vel;

    bool temp_drop_detected = (temp_drop > 1.0f);
    bool vel_spike_detected = (vel_spike > 45.0f);
    bool hum_change_detected = (fabs(hum_change) > 5.0f);

    if (temp_drop_detected && (vel_spike_detected || hum_change_detected)) {
        window_open_confirmations++;
        printf("[WINDOW] Detection: drop=%.1f°C vel_spike=%.0fcm/s hum_delta=%.1f%% (conf=%d)\n",
               temp_drop, vel_spike, hum_change, window_open_confirmations);
    } else {
        if (window_open_confirmations > 0)
            window_open_confirmations--;
    }

    if (window_open_confirmations >= 2) {
        return 1; /* WINDOW_OPEN */
    }

    /* Check if window was open and now closed (temp stabilizing) */
    if (latest_data.window_state == 1 &&
        fabs(temp_drop) < 0.2f && current_air_vel < 15.0f) {
        return 0; /* WINDOW_CLOSED */
    }

    return latest_data.window_state;
}

/* ---- Solar gain estimation ---- */

static uint8_t estimate_solar_gain(float max_surface_temp, uint16_t light_lux)
{
    /* If a sunlit surface (window, wall) has temp > ambient + 5°C,
     * and light > 500 lux, estimate solar gain.
     * Gain ≈ U-value × area × (surface_temp - ambient)
     * Stub: map light to watts (0-255 range) */
    if (light_lux > 500 && max_surface_temp > 25.0f) {
        return (uint8_t)(light_lux / 20);
    }
    return 0;
}

/* ---- ADC reading (battery, solar, light) ---- */

static uint16_t adc_read(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    /* In production: configure ADC channel, start conversion, read */
    return 2048; /* mid-range stub */
}

static uint8_t compute_battery_pct(uint16_t batt_raw)
{
    /* 2× AA: 3.0V fresh, 2.0V dead
     * Voltage divider: Vbatt = Vref * raw / 4095 * divider_ratio
     * Map 3.0V→100%, 2.0V→0% */
    float voltage = (batt_raw / 4095.0f) * 3.3f * 2.0f; /* ÷2 divider */
    float pct = (voltage - 2.0f) / 1.0f * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    return (uint8_t)pct;
}

static uint8_t compute_solar_mv(uint16_t solar_raw)
{
    float voltage = (solar_raw / 4095.0f) * 3.3f * 2.0f;
    return (uint8_t)(voltage * 100.0f); /* ×10 mV */
}

/* ---- Build and send sensor data packet ---- */

static void build_sensor_data(sensor_data_payload_t *data)
{
    float temp, humidity, mrt, max_surface, air_vel, pressure, bmp_temp;

    sht45_read(&temp, &humidity);
    mlx90640_read(&mrt, &max_surface, NULL);
    sdp810_read(&air_vel);
    bmp390_read(&pressure, &bmp_temp);

    uint16_t light_lux = als_read_lux();
    uint16_t co2 = scd41_read_co2();
    uint8_t solar_gain = estimate_solar_gain(max_surface, light_lux);

    uint8_t occ_state, occ_conf;
    occupancy_detect(&occ_state, &occ_conf);

    int8_t window_state = detect_window_open(temp, humidity, air_vel);

    /* Update previous values for next window detection */
    prev_temp = temp;
    prev_humidity = humidity;
    prev_air_vel = air_vel;
    prev_measure_time = get_absolute_time();

    /* Fill payload */
    memset(data, 0, sizeof(*data));
    data->air_temp_cx100    = (int16_t)(temp * 100);
    data->mrt_cx100        = (int16_t)(mrt * 100);
    data->humidity_centi    = (uint16_t)(humidity * 100);
    data->air_vel_cms_x100 = (int16_t)(air_vel * 100);
    data->pressure_pa     = (int16_t)(pressure * 100); /* hPa → truncated */
    data->occupancy        = occ_state;
    data->occupancy_conf   = occ_conf;
    data->light_lux       = light_lux;
    data->co2_ppm         = co2;
    data->window_state    = window_state;
    data->solar_gain_w    = solar_gain;
    data->battery_pct     = compute_battery_pct(adc_read(NULL, 0));
    data->solar_mv        = compute_solar_mv(adc_read(NULL, 1));
    data->signal_rssi     = -70;
    data->fault_flags     = 0;
    data->seq_num         = seq_num++;
}

/* ---- Radio (Sub-GHz via STM32WL integrated SX126x) ---- */

static void radio_init(void)
{
    /* In production: initialize SUBGHZ PhY
     * Set frequency: 915 MHz (US) or 868 MHz (EU)
     * Set modulation: LoRa SF7, BW 125kHz, coding 4/5
     * Set TX power: +14 dBm
     * Set sync word: 0x5447 ("TG")
     */
    printf("[RADIO] STM32WL Sub-GHz initialized (915MHz, LoRa SF7)\n");
}

static void radio_send(const uint8_t *data, uint16_t len)
{
    /* In production: SUBGHZSPI write FIFO, set TX, wait TxDone */
    printf("[RADIO] TX %d bytes\n", len);
}

static int16_t radio_receive(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms)
{
    /* In production: set RX, wait RxDone, read FIFO */
    return 0; /* stub */
}

/* ---- Send telemetry to hub ---- */

static void send_telemetry(void)
{
    build_sensor_data(&latest_data);

    mesh_packet_t pkt;
    uint16_t pkt_len = mesh_build_packet(
        MY_NODE_ID, NODE_ID_HUB, PKT_SENSOR_DATA,
        (uint8_t *)&latest_data, sizeof(latest_data), &pkt);
    radio_send((uint8_t *)&pkt, pkt_len);

    /* If window state changed, also send immediate WINDOW_OPEN alert */
    if (latest_data.window_state == 1) {
        window_open_payload_t wo;
        wo.room_id = MY_NODE_ID;
        wo.confidence = 200;
        wo.temp_drop_cx100 = (int16_t)((prev_temp - (latest_data.air_temp_cx100 / 100.0f)) * 100);
        wo.air_vel_cms_x100 = latest_data.air_vel_cms_x100;
        wo.duration_s = 0;
        memset(wo.reserved, 0, sizeof(wo.reserved));

        pkt_len = mesh_build_packet(
            MY_NODE_ID, NODE_ID_HUB, PKT_WINDOW_OPEN,
            (uint8_t *)&wo, sizeof(wo), &pkt);
        radio_send((uint8_t *)&pkt, pkt_len);
        printf("[ALERT] Sent WINDOW_OPEN alert\n");
    }

    /* If freeze risk, send FREEZE_ALERT */
    float temp = latest_data.air_temp_cx100 / 100.0f;
    if (temp < 4.0f) {
        freeze_alert_payload_t fa;
        fa.alert_level = ALERT_EMERGENCY;
        fa.room_id = MY_NODE_ID;
        fa.temp_cx100 = latest_data.air_temp_cx100;
        fa.mrt_cx100 = latest_data.mrt_cx100;
        fa.all_valves_open = 0;
        fa.boiler_relay_on = 0;
        fa.timestamp_s = 0;
        memset(fa.reserved, 0, sizeof(fa.reserved));

        pkt_len = mesh_build_packet(
            MY_NODE_ID, NODE_ID_BROADCAST, PKT_FREEZE_ALERT,
            (uint8_t *)&fa, sizeof(fa), &pkt);
        radio_send((uint8_t *)&pkt, pkt_len);
        printf("[ALERT] Sent FREEZE_ALERT (T=%.1f°C)\n", temp);
    }

    printf("[TELEMETRY] T=%.1f°C MRT=%.1f°C H=%.0f%% VEL=%.0fcm/s OCC=%d WIN=%d SOL=%dW BAT=%d\n",
           latest_data.air_temp_cx100 / 100.0f,
           latest_data.mrt_cx100 / 100.0f,
           latest_data.humidity_centi / 100.0f,
           latest_data.air_vel_cms_x100 / 100.0f,
           latest_data.occupancy,
           latest_data.window_state,
           latest_data.solar_gain_w,
           latest_data.battery_pct);
}

/* ---- Listen for hub commands ---- */

static void process_command(mesh_packet_t *pkt)
{
    if (pkt->pkt_type == PKT_COMMAND) {
        command_payload_t *cmd = (command_payload_t *)pkt->payload;
        switch (cmd->cmd_type) {
        case CMD_CALIBRATE:
            if (cmd->param_len >= 5) {
                uint8_t sensor = cmd->params[0];
                int32_t value;
                memcpy(&value, &cmd->params[1], 4);
                printf("[CAL] sensor=%d value=%d\n", sensor, value);
                if (sensor == 0) cal.temp_offset = value / 100.0f;
                else if (sensor == 1) cal.humidity_offset = value / 100.0f;
                else if (sensor == 2) cal.mrt_offset = value / 100.0f;
            }
            break;

        case CMD_ENROLL_SENSOR:
            /* Hub assigns this node a permanent ID and zone */
            printf("[ENROLL] Enrolled in mesh\n");
            break;

        default:
            printf("[CMD] Unknown command 0x%02X\n", cmd->cmd_type);
            break;
        }
    }
}

static void listen_for_commands(void)
{
    uint8_t rx_buf[128];
    int16_t rx_len = radio_receive(rx_buf, sizeof(rx_buf), 50); /* 50ms listen */
    if (rx_len > 0) {
        mesh_packet_t pkt;
        if (mesh_parse_packet(rx_buf, rx_len, &pkt) == 0) {
            if (pkt.dst_id == MY_NODE_ID || pkt.dst_id == NODE_ID_BROADCAST) {
                process_command(&pkt);
            }
        }
    }
}

/* ---- Deep sleep / power management ---- */

static void enter_deep_sleep(uint32_t seconds)
{
    /* In production:
     * 1. Stop all sensors (power gate via MOSFET)
     * 2. Configure RTC wakeup after 'seconds'
     * 3. Enable PIR interrupt wakeup (AM612 on EXTI line)
     * 4. Enter Stop2 mode (lowest power with RTC + GPIO wakeup)
     *    Current: ~3µA in Stop2
     * 5. On wakeup (RTC or PIR), re-init sensors, measure, send
     */
    printf("[SLEEP] Entering deep sleep for %lu seconds\n", seconds);
    HAL_Delay(10);
    /* HAL_PWR_EnterSTOPMode(...) */
}

/* ---- Main ---- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Initialize I2C, UART, ADC, GPIO */
    /* MX_I2C1_Init(); — MLX90640 */
    /* MX_I2C2_Init(); — SHT45, BMP390, SCD41 */
    /* MX_USART1_UART_Init(); — mmWave */
    /* MX_ADC_Init(); — light, battery, solar */

    printf("\n=== ThermoGrid Room Sensor v1.0 (ID=0x%02X) ===\n", MY_NODE_ID);

    radio_init();

    /* PIR interrupt configuration */
    /* GPIO EXTI on PA0 (AM612), rising edge */

    uint32_t poll_interval_s = 30; /* normal: 30s */

    while (1) {
        /* Measure all sensors and send telemetry */
        send_telemetry();

        /* Listen for hub commands (brief) */
        listen_for_commands();

        /* Adjust poll rate:
         * - During daylight: faster (solar gain changes, more dynamic)
         * - At night: slower (temps stable, save battery)
         * - If window open: fast (10s) to track recovery
         */
        if (latest_data.window_state == 1) {
            poll_interval_s = 10; /* fast poll when window open */
        } else if (latest_data.light_lux > 100) {
            poll_interval_s = 30; /* daylight: 30s */
        } else {
            poll_interval_s = 60; /* night: 60s */
        }

        /* If battery low, slow down */
        if (latest_data.battery_pct < 20) {
            poll_interval_s = 120; /* conserve: 2 min */
        }

        /* Deep sleep until next poll or PIR wakeup */
        enter_deep_sleep(poll_interval_s);

        /* On wakeup: if PIR triggered, measure immediately
         * (occupancy change needs fast reporting) */
    }
}
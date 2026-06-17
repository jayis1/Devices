/**
 * @file main.c
 * @brief SoundNest Room Acoustic Sensor — Main entry point.
 *
 * The room sensor features a 4-microphone array for sound source
 * localization, TinyML sound classification, SPL measurement, and
 * environmental sensing (temp, humidity, light, PIR).
 *
 * Runs on nRF52840 with SX1262 Sub-GHz radio for mesh communication.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>
#include <math.h>

#include "../../../common/protocol/mesh_packet.h"
#include "../../../common/dsp/spl.h"

/* ── Logging ────────────────────────────────────────────────────────── */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(room_sensor, LOG_LEVEL_INF);

/* ── Configuration ──────────────────────────────────────────────────── */

#define MIC_SAMPLE_RATE      16000
#define MIC_NUM_CHANNELS     4
#define MIC_BITS_PER_SAMPLE  24
#define MIC_WINDOW_MS        2000   /* 2-second classification window */
#define MIC_WINDOW_SAMPLES   (MIC_SAMPLE_RATE * MIC_WINDOW_MS / 1000)
#define SPL_REPORT_INTERVAL_MS  5000  /* 5-second SPL reports */
#define HEARTBEAT_INTERVAL_MS  60000  /* 1-minute heartbeats */
#define MESH_TX_POWER_DBM      10
#define MESH_CHANNEL           0

/* ── Thread Priorities ──────────────────────────────────────────────── */

#define THREAD_PRIO_MIC        5   /* Highest: audio capture */
#define THREAD_PRIO_CLASSIFY    4   /* High: ML inference */
#define THREAD_PRIO_MESH_TX    3   /* Medium: mesh transmit */
#define THREAD_PRIO_SENSORS    2   /* Low: environmental sensors */
#define THREAD_PRIO_POWER      1   /* Lowest: power management */

/* ── Thread Stack Sizes ─────────────────────────────────────────────── */

#define STACK_MIC          4096
#define STACK_CLASSIFY     6144
#define STACK_MESH_TX     2048
#define STACK_SENSORS     2048
#define STACK_POWER       1024

/* ── Global State ──────────────────────────────────────────────────── */

typedef struct {
    /* Node identity */
    uint16_t node_addr;
    uint8_t  tdma_slot;
    uint16_t seq_num;

    /* SPL measurement */
    spl_calculator_t spl_calc;
    float current_spl_dba;
    float current_spl_dbc;
    float current_spl_dbz;
    float spl_min_dba;
    float spl_max_dba;

    /* Sound classification */
    uint8_t  last_sound_class;
    uint8_t  last_confidence;
    int8_t   last_direction_deg;

    /* Environmental sensors */
    float temp_c;
    float humidity_pct;
    float light_lux;
    bool  occupancy;

    /* Power */
    uint16_t battery_mv;
    bool  usb_powered;

    /* Mesh */
    bool mesh_joined;
    bool mesh_busy;

    /* Audio buffers */
    int16_t *mic_buffer[MIC_NUM_CHANNELS];
    int16_t *classify_buffer;
    float   *float_buffer;

    /* TDOA localization */
    int16_t  tdoa_samples[4];  /* Time difference of arrival (in samples) */
    float    source_azimuth;    /* Estimated azimuth in degrees */
    float    source_confidence; /* Localization confidence */
} sensor_state_t;

static sensor_state_t g_state;

/* ── Thread IDs ─────────────────────────────────────────────────────── */

static k_tid_t tid_mic, tid_classify, tid_mesh_tx, tid_sensors, tid_power;

/* ── Semaphores and Queues ──────────────────────────────────────────── */

static struct k_sem sem_mic_data;      /* Signals new mic data available */
static struct k_sem sem_classify;      /* Signals classification done */
static struct k_mutex state_mutex;

K_MSGQ_DEFINE(event_msgq, sizeof(event_report_payload_t), 8, 4);
K_MSGQ_DEFINE(spl_msgq, sizeof(spl_report_payload_t), 4, 4);

/* ── Microphone Array Configuration ─────────────────────────────────── */

/* I2S configuration for 4× SPH0645LM4H-6 MEMS microphones
 * All mics share BCLK and LRCLK, each has separate data output
 * Microphone geometry: square, 40mm spacing
 *
 *   MIC1 ─── 40mm ─── MIC2
 *    │                   │
 *   40mm               40mm
 *    │                   │
 *   MIC3 ─── 40mm ─── MIC4
 */

#define MIC_ARRAY_SPACING_MM  40.0f
#define SOUND_SPEED_MM_PER_MS  343200.0f  /* mm/s at 20°C */

/* ── SX1262 Radio Interface ─────────────────────────────────────────── */

/* SPI device for SX1262 */
static const struct device *sx1262_spi;

/* ── Mic Capture Thread ─────────────────────────────────────────────── */

static void thread_mic_capture(void *p1, void *p2, void *p3)
{
    LOG_INF("Mic capture thread started");

    int16_t *interleaved = k_malloc(MIC_WINDOW_SAMPLES * MIC_NUM_CHANNELS * 2);
    if (!interleaved) {
        LOG_ERR("Failed to allocate mic interleaved buffer");
        return;
    }

    while (1) {
        /* Read from I2S (4-channel interleaved) */
        /* In practice, 4 I2S streams are captured in sequence or via
         * TDM mode on nRF52840's I2S peripheral */
        size_t bytes_received = 0;

        /* Capture 2-second window from mic array */
        for (int ch = 0; ch < MIC_NUM_CHANNELS; ch++) {
            /* Copy channel data from interleaved buffer */
            /* Real implementation would use nRF52840 I2S with
             * DMA and double-buffering */
            for (int i = 0; i < MIC_WINDOW_SAMPLES; i++) {
                g_state.mic_buffer[ch][i] =
                    interleaved[i * MIC_NUM_CHANNELS + ch] >> 8;  /* 24→16 bit */
            }
        }

        /* Compute TDOA for sound localization */
        /* Cross-correlation between mic pairs to find time delays */
        float sum_cc = 0;
        int max_lag = 0;
        float max_cc = -1e30f;

        for (int lag = -64; lag <= 64; lag++) {
            sum_cc = 0;
            int count = 0;
            for (int i = 0; i < MIC_WINDOW_SAMPLES - 256; i++) {
                int idx1 = i + 128;
                int idx2 = i + 128 + lag;
                if (idx2 >= 0 && idx2 < MIC_WINDOW_SAMPLES) {
                    sum_cc += (float)g_state.mic_buffer[0][idx1] *
                              (float)g_state.mic_buffer[1][idx2];
                    count++;
                }
            }
            sum_cc /= (float)count;
            if (sum_cc > max_cc) {
                max_cc = sum_cc;
                max_lag = lag;
            }
        }

        /* Convert lag to time difference */
        float tdoa_sec = (float)max_lag / (float)MIC_SAMPLE_RATE;

        /* Convert time difference to angle using mic spacing */
        /* sin(θ) = (v * Δt) / d */
        float sin_theta = (SOUND_SPEED_MM_PER_MS * tdoa_sec * 1000.0f) /
                          MIC_ARRAY_SPACING_MM;
        if (sin_theta > 1.0f) sin_theta = 1.0f;
        if (sin_theta < -1.0f) sin_theta = -1.0f;
        g_state.source_azimuth = asinf(sin_theta) * 180.0f / M_PI;
        g_state.source_confidence = fminf(max_cc / 1e10f, 1.0f);

        /* Signal that mic data is ready for classification */
        k_sem_give(&sem_mic_data);

        /* Sleep until next window */
        k_msleep(MIC_WINDOW_MS);
    }
}

/* ── Sound Classification Thread ─────────────────────────────────────── */

static void thread_classify(void *p1, void *p2, void *p3)
{
    LOG_INF("Classification thread started");

    while (1) {
        /* Wait for mic data */
        k_sem_take(&sem_mic_data, K_FOREVER);

        /* Convert to float for SPL calculation */
        for (int i = 0; i < MIC_WINDOW_SAMPLES; i++) {
            g_state.float_buffer[i] =
                (float)g_state.mic_buffer[0][i] / 32768.0f;
        }

        /* Compute SPL */
        spl_result_t spl_result;
        spl_process(&g_state.spl_calc, g_state.mic_buffer[0],
                    MIC_WINDOW_SAMPLES, &spl_result);

        k_mutex_lock(&state_mutex, K_FOREVER);
        g_state.current_spl_dba = spl_result.spl_dba;
        g_state.current_spl_dbc = spl_result.spl_dbc;
        g_state.current_spl_dbz = spl_result.spl_dbz;

        if (spl_result.spl_dba < g_state.spl_min_dba || g_state.spl_min_dba < -100.0f)
            g_state.spl_min_dba = spl_result.spl_dba;
        if (spl_result.spl_dba > g_state.spl_max_dba)
            g_state.spl_max_dba = spl_result.spl_dba;
        k_mutex_unlock(&state_mutex);

        /* Run TinyML classification if SPL is above threshold */
        if (spl_result.spl_dba > 35.0f) {
            /* In production, this calls TensorFlow Lite Micro inference
             * on the 2-second mel-spectrogram input.
             * Model: 40-class sound event classifier, int8 quantized,
             * ~200KB flash, ~50KB RAM on nRF52840. */

            /* Placeholder: use energy-based heuristic */
            uint8_t sound_class = SOUND_UNKNOWN;
            uint8_t confidence = 0;

            if (spl_result.spl_dba > 85.0f) {
                sound_class = SOUND_SMOKE_ALARM;  /* Likely alarm */
                confidence = 60;
            } else if (spl_result.spl_dba > 70.0f) {
                sound_class = SOUND_SPEECH;  /* Likely speech */
                confidence = 70;
            } else if (spl_result.spl_dba > 55.0f) {
                sound_class = SOUND_TV;  /* Likely media */
                confidence = 50;
            } else {
                sound_class = SOUND_UNKNOWN;
                confidence = 30;
            }

            k_mutex_lock(&state_mutex, K_FOREVER);
            g_state.last_sound_class = sound_class;
            g_state.last_confidence = confidence;
            g_state.last_direction_deg = (int8_t)g_state.source_azimuth;
            k_mutex_unlock(&state_mutex);

            /* Send event report if confidence is high enough */
            if (confidence >= 50 && sound_class != SOUND_UNKNOWN) {
                event_report_payload_t event = {
                    .sound_class = sound_class,
                    .confidence = confidence,
                    .direction_deg = (int8_t)g_state.source_azimuth,
                    .spl_dba = (uint8_t)spl_result.spl_dba,
                    .spl_dbc = (uint8_t)spl_result.spl_dbc,
                    .spl_dbz = (uint8_t)spl_result.spl_dbz,
                    .peak_spl = (uint8_t)spl_result.spl_peak_dba,
                    .duration_ms = MIC_WINDOW_MS,
                    .timestamp = (uint32_t)k_uptime_get(),
                    .room_id = 0,  /* Configured per room */
                    .occupancy = g_state.occupancy ? 1 : 0,
                    .temp_c = g_state.temp_c,
                    .humidity_pct = (uint8_t)g_state.humidity_pct,
                };
                k_msgq_put(&event_msgq, &event, K_NO_WAIT);
            }
        }

        /* Signal classification complete */
        k_sem_give(&sem_classify);
    }
}

/* ── Mesh Transmit Thread ─────────────────────────────────────────────── */

static void thread_mesh_tx(void *p1, void *p2, void *p3)
{
    LOG_INF("Mesh TX thread started");

    /* Join the mesh network */
    while (!g_state.mesh_joined) {
        join_req_payload_t join = {
            .node_type = NODE_TYPE_ROOM_SENSOR,
            .firmware_ver = {1, 0, 0, 0},
            .capabilities = CAPABILITY_SPL_METER | CAPABILITY_MIC_ARRAY |
                           CAPABILITY_CLASSIFIER | CAPABILITY_LOCALIZER |
                           CAPABILITY_TEMP_HUMID | CAPABILITY_LIGHT |
                           CAPABILITY_PIR,
            .hw_revision = 1,
        };

        uint8_t buf[128];
        mesh_header_t header = {
            .sync_word = MESH_SYNC_WORD,
            .src_addr = 0x0000,  /* Unassigned */
            .dst_addr = 0x0001,  /* Hub */
            .msg_type = MSG_TYPE_JOIN_REQ,
            .seq_num = g_state.seq_num++,
        };

        int len = mesh_packet_encode(&header, (const uint8_t *)&join,
                                      sizeof(join_req_payload_t),
                                      buf, sizeof(buf));
        if (len > 0) {
            /* Send via SX1262 */
            sx1262_send(buf, len);
            LOG_INF("Join request sent");
        }

        /* Wait for JOIN_ACK (handled in mesh RX ISR) */
        k_msleep(5000);
    }

    /* Main transmission loop */
    while (1) {
        /* Wait for TDMA slot (synchronized by hub) */
        k_msleep(100);  /* Placeholder for TDMA synchronization */

        /* Send event reports */
        event_report_payload_t event;
        if (k_msgq_get(&event_msgq, &event, K_NO_WAIT) == 0) {
            uint8_t buf[128];
            int len = mesh_build_event_report(g_state.node_addr, 0x0001,
                                               g_state.seq_num++,
                                               &event, buf, sizeof(buf));
            if (len > 0) {
                sx1262_send(buf, len);
                LOG_INF("Event sent: class=%d conf=%d%% dBA=%d",
                        event.sound_class, event.confidence, event.spl_dba);
            }
        }

        /* Periodic SPL report */
        static int64_t last_spl_report = 0;
        if (k_uptime_get() - last_spl_report > SPL_REPORT_INTERVAL_MS) {
            last_spl_report = k_uptime_get();

            spl_report_payload_t spl = {
                .spl_dba = (uint8_t)g_state.current_spl_dba,
                .spl_dbc = (uint8_t)g_state.current_spl_dbc,
                .spl_dbz = (uint8_t)g_state.current_spl_dbz,
                .spl_min = (uint8_t)g_state.spl_min_dba,
                .spl_max = (uint8_t)g_state.spl_max_dba,
                .spl_eq = (uint8_t)g_state.spl_calc.stats.leq_dba,
                .active_events = 0,  /* Bitfield of currently active sounds */
                .occupancy = g_state.occupancy ? 1 : 0,
                .temp_c = g_state.temp_c,
                .humidity_pct = (uint8_t)g_state.humidity_pct,
                .timestamp = (uint32_t)k_uptime_get(),
                .battery_mv = g_state.battery_mv,
            };

            /* Fill 1/3-octave spectrum */
            for (int i = 0; i < SPL_NUM_BANDS && i < 32; i++) {
                spl.spectral_32[i] =
                    (uint8_t)fminf(fmaxf(g_state.spl_calc.stats.leq_dba - 20.0f, 0.0f), 255.0f);
            }

            uint8_t buf[128];
            int len = mesh_build_spl_report(g_state.node_addr, 0x0001,
                                             g_state.seq_num++,
                                             &spl, buf, sizeof(buf));
            if (len > 0) {
                sx1262_send(buf, len);
                LOG_INF("SPL report sent: %.1f dBA", g_state.current_spl_dba);
            }

            /* Reset min/max for next interval */
            g_state.spl_min_dba = 200.0f;
            g_state.spl_max_dba = -120.0f;
        }

        /* Send heartbeat */
        static int64_t last_heartbeat = 0;
        if (k_uptime_get() - last_heartbeat > HEARTBEAT_INTERVAL_MS) {
            last_heartbeat = k_uptime_get();

            heartbeat_payload_t hb = {
                .node_type = NODE_TYPE_ROOM_SENSOR,
                .battery_mv = g_state.battery_mv,
                .rssi = sx1262_get_rssi(),
                .status = 0x1F,  /* All subsystems OK */
                .uptime_sec = (uint32_t)(k_uptime_get() / 1000),
                .events_today = 0,  /* Would be tracked */
                .packets_sent = g_state.seq_num,
                .packets_missed = 0,
            };

            uint8_t buf[128];
            int len = mesh_build_heartbeat(g_state.node_addr, 0x0001,
                                            g_state.seq_num++, &hb,
                                            buf, sizeof(buf));
            if (len > 0) {
                sx1262_send(buf, len);
            }
        }

        k_msleep(100);
    }
}

/* ── Environmental Sensors Thread ─────────────────────────────────────── */

static void thread_sensors(void *p1, void *p2, void *p3)
{
    LOG_INF("Sensors thread started");

    while (1) {
        /* Read SHT40 temperature/humidity */
        /* Real implementation uses Zephyr sensor API */
        const struct device *sht40 = DEVICE_DT_GET(sensor_sht40);
        if (device_is_ready(sht40)) {
            struct sensor_value temp, humidity;
            sensor_sample_fetch(sht40);
            sensor_channel_get(sht40, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            sensor_channel_get(sht40, SENSOR_CHAN_HUMIDITY, &humidity);

            k_mutex_lock(&state_mutex, K_FOREVER);
            g_state.temp_c = sensor_value_to_double(&temp);
            g_state.humidity_pct = sensor_value_to_double(&humidity);
            k_mutex_unlock(&state_mutex);
        }

        /* Read VEML7700 light sensor */
        const struct device *veml7700 = DEVICE_DT_GET(sensor_veml7700);
        if (device_is_ready(veml7700)) {
            struct sensor_value light;
            sensor_sample_fetch(veml7700);
            sensor_channel_get(veml7700, SENSOR_CHAN_LIGHT, &light);

            k_mutex_lock(&state_mutex, K_FOREVER);
            g_state.light_lux = sensor_value_to_double(&light);
            k_mutex_unlock(&state_mutex);
        }

        /* Read AM312 PIR motion sensor */
        const struct gpio_dt_spec pir = GPIO_DT_SPEC_GET(DT_NODELABEL(pir), gpios);
        bool pir_state = gpio_pin_get_dt(&pir);

        k_mutex_lock(&state_mutex, K_FOREVER);
        g_state.occupancy = pir_state;
        k_mutex_unlock(&state_mutex);

        /* Read battery voltage */
        /* nRF52840 internal ADC on battery pin */
        /* Real implementation uses nrfx_saadc */
        uint16_t vbat = 3000;  /* Placeholder: 3.0V = good battery */
        k_mutex_lock(&state_mutex, K_FOREVER);
        g_state.battery_mv = vbat;
        k_mutex_unlock(&state_mutex);

        /* Check USB power */
        const struct gpio_dt_spec usb_detect = GPIO_DT_SPEC_GET(DT_NODELABEL(usb_detect), gpios);
        k_mutex_lock(&state_mutex, K_FOREVER);
        g_state.usb_powered = gpio_pin_get_dt(&usb_detect);
        k_mutex_unlock(&state_mutex);

        k_msleep(5000);  /* Read sensors every 5 seconds */
    }
}

/* ── Power Management Thread ─────────────────────────────────────────── */

static void thread_power(void *p1, void *p2, void *p3)
{
    LOG_INF("Power management thread started");

    while (1) {
        k_mutex_lock(&state_mutex, K_FOREVER);
        bool usb = g_state.usb_powered;
        uint16_t bat = g_state.battery_mv;
        k_mutex_unlock(&state_mutex);

        if (!usb) {
            /* On battery power: optimize for long life */
            if (bat < 2700) {
                /* Critical battery: reduce sampling rate */
                LOG_WRN("Low battery: %dmV, reducing activity", bat);
                /* Increase measurement intervals */
            }

            /* Enter low-power sleep between measurements
             * nRF52840 can drop to ~1.5µA in System OFF mode
             * Wake on: mic activity above threshold, PIR motion, timer */
            /* Real implementation uses nRF power management */
        }

        k_msleep(10000);  /* Check power every 10 seconds */
    }
}

/* ── Main Entry Point ────────────────────────────────────────────────── */

int main(void)
{
    LOG_INF("╔══════════════════════════════════════╗");
    LOG_INF("║   SoundNest Room Sensor v1.0         ║");
    LOG_INF("║   4-Mic Array + TinyML Classifier    ║");
    LOG_INF("╚══════════════════════════════════════╝");

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.spl_min_dba = 200.0f;
    g_state.spl_max_dba = -120.0f;

    /* Initialize SPL calculator */
    spl_init(&g_state.spl_calc);

    /* Allocate audio buffers */
    for (int i = 0; i < MIC_NUM_CHANNELS; i++) {
        g_state.mic_buffer[i] = k_malloc(MIC_WINDOW_SAMPLES * sizeof(int16_t));
        if (!g_state.mic_buffer[i]) {
            LOG_ERR("Failed to allocate mic buffer %d", i);
            return -1;
        }
    }
    g_state.classify_buffer = k_malloc(MIC_WINDOW_SAMPLES * sizeof(int16_t));
    g_state.float_buffer = k_malloc(MIC_WINDOW_SAMPLES * sizeof(float));

    /* Initialize synchronization objects */
    k_sem_init(&sem_mic_data, 0, 1);
    k_sem_init(&sem_classify, 0, 1);
    k_mutex_init(&state_mutex);

    /* Initialize SX1262 Sub-GHz radio */
    sx1262_spi = DEVICE_DT_GET(DT_NODELABEL(sx1262));
    if (!device_is_ready(sx1262_spi)) {
        LOG_ERR("SX1262 SPI not ready");
        return -1;
    }
    sx1262_init();
    sx1262_set_frequency(868000000);  /* 868MHz EU */
    sx1262_set_tx_power(MESH_TX_POWER_DBM);
    sx1262_set_spreading_factor(7);  /* SF7 for urban */

    /* Initialize GPIO */
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
    gpio_pin_configure_dt(&button, GPIO_INPUT);

    /* Start threads */
    k_thread_create(&tid_mic, NULL, STACK_MIC,
                     thread_mic_capture, NULL, NULL, NULL,
                     THREAD_PRIO_MIC, 0, K_NO_WAIT);
    k_thread_create(&tid_classify, NULL, STACK_CLASSIFY,
                     thread_classify, NULL, NULL, NULL,
                     THREAD_PRIO_CLASSIFY, 0, K_NO_WAIT);
    k_thread_create(&tid_mesh_tx, NULL, STACK_MESH_TX,
                     thread_mesh_tx, NULL, NULL, NULL,
                     THREAD_PRIO_MESH_TX, 0, K_NO_WAIT);
    k_thread_create(&tid_sensors, NULL, STACK_SENSORS,
                     thread_sensors, NULL, NULL, NULL,
                     THREAD_PRIO_SENSORS, 0, K_NO_WAIT);
    k_thread_create(&tid_power, NULL, STACK_POWER,
                     thread_power, NULL, NULL, NULL,
                     THREAD_PRIO_POWER, 0, K_NO_WAIT);

    LOG_INF("All threads started. Room sensor is running.");

    return 0;
}

/* ── SX1262 Radio Stubs ───────────────────────────────────────────────── */
/* (Real implementation uses Zephyr SPI driver) */

static int sx1262_init(void) { return 0; }
static int sx1262_send(const uint8_t *data, size_t len) { return 0; }
static int8_t sx1262_get_rssi(void) { return -60; }
static void sx1262_set_frequency(uint32_t freq) { (void)freq; }
static void sx1262_set_tx_power(int8_t dbm) { (void)dbm; }
static void sx1262_set_spreading_factor(uint8_t sf) { (void)sf; }
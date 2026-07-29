/*
 * RehabSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the BLE body-area network, bridges to the cloud
 * via Wi-Fi/MQTT, runs local edge ExerciseNet + FormNet + RepCount
 * inference, drives the TFT display + speaker + haptic, and manages
 * OTA firmware distribution to all nodes.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "RehabSync-Hub";

/* === Global state === */
static rs_mesh_ctx_t g_mesh;
static QueueHandle_t g_imu_queue;       /* IMU samples from body sensors */
static QueueHandle_t g_force_queue;     /* Force samples from smart band */
static QueueHandle_t g_pressure_queue;  /* Pressure frames from mat */
static QueueHandle_t g_feedback_queue;  /* Feedback commands (audio/haptic) */
static SemaphoreHandle_t g_radio_mutex;

/* Session state */
static uint8_t g_session_active = 0;
static uint8_t g_current_exercise = 0;
static uint8_t g_target_reps = 10;
static uint16_t g_rep_count = 0;
static uint8_t g_form_score = 100;
static uint8_t g_form_deviation = 0;
static uint32_t g_session_start_time = 0;

/* Connected sensors */
static uint8_t g_body_sensors_connected = 0;
static uint8_t g_smart_band_connected = 0;
static uint8_t g_pressure_mat_connected = 0;

/* Joint angles (degrees) from sensor pairs */
static float g_joint_angles[6];  /* up to 6 joint angles */
static float g_rom_current[6];   /* current range of motion per joint */
static float g_rom_max[6];       /* max ROM achieved this session */

/* Force data */
static int32_t g_current_force_mg = 0;  /* milligrams-force */
static int32_t g_peak_force_mg = 0;
static int32_t g_total_volume_mg = 0;

/* Pressure mat data */
static uint16_t g_cop_x = 0;
static uint16_t g_cop_y = 0;
static uint16_t g_total_weight_g = 0;
static uint16_t g_asymmetry = 0;

/* ExerciseNet inference result */
static uint8_t g_exercise_id = 0;
static uint8_t g_exercise_confidence = 0;

/* === SX1262 SPI Interface (ESP32-S3) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUB_GPIO_SX_MOSI,
        .miso_io_num = HUB_GPIO_SX_MISO,
        .sclk_io_num = HUB_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = HUB_GPIO_SX_NSS,
        .queue_size = 7,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

/* === I²C Init === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_I2C_SDA,
        .scl_io_num = HUB_GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === SX1262 platform interface === */
int sx1262_spi_write(sx1262_t *radio, uint8_t cmd, const uint8_t *data, size_t len)
{
    uint8_t tx_buf[260] = { cmd };
    if (data && len > 0) memcpy(tx_buf + 1, data, len);
    spi_transaction_t t = {
        .length = (8 * (1 + len)),
        .tx_buffer = tx_buf,
    };
    xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
    esp_err_t ret = spi_device_polling_transmit(g_spi_dev, &t);
    xSemaphoreGive(g_radio_mutex);
    return (ret == ESP_OK) ? 0 : -1;
}

int sx1262_spi_read(sx1262_t *radio, uint8_t cmd, uint8_t *data, size_t len)
{
    uint8_t tx_buf[260] = { cmd };
    uint8_t rx_buf[260] = {0};
    spi_transaction_t t = {
        .length = 8 * (1 + len),
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
    esp_err_t ret = spi_device_polling_transmit(g_spi_dev, &t);
    xSemaphoreGive(g_radio_mutex);
    if (ret == ESP_OK && data) memcpy(data, rx_buf + 1, len);
    return (ret == ESP_OK) ? 0 : -1;
}

void sx1262_reset(sx1262_t *radio)
{
    gpio_set_direction(HUB_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(HUB_GPIO_SX_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(HUB_GPIO_SX_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* === BLE BAN Coordinator === */
/* ESP32-S3 BLE central: connects to up to 7 peripherals (6 body sensors + smart band)
 * Uses NimBLE stack. GATT service UUID for RehabSync IMU streaming:
 *   IMU Service:  0xRS01 (custom)
 *   Force Service: 0xRS02
 *   Characteristics stream IMU/force data at 100/50 Hz
 */

/* IMU sample from BLE */
typedef struct {
    uint8_t  sensor_id;
    rs_imu_sample_t imu;
    uint32_t timestamp;
} imu_event_t;

/* Force sample from BLE */
typedef struct {
    rs_force_sample_t force;
    uint32_t timestamp;
} force_event_t;

/* Pressure frame from Sub-GHz */
typedef struct {
    rs_pressure_header_t header;
    uint16_t cells[64];  /* compressed 16x16 → 8x8 averaged for hub processing */
} pressure_event_t;

/* === Tasks === */

/* BLE Central Task: manages connections to body sensors + smart band */
static void ble_central_task(void *arg)
{
    ESP_LOGI(TAG, "BLE central task started");
    /* In production: NimBLE GAP central, GATT discovery, notification registration
     * For each body sensor:
     *   1. Scan for RS_SYNC0/RS_SYNC1 in advertising data
     *   2. Connect with 10ms interval
     *   3. Discover IMU service + characteristic
     *   4. Enable notifications → receive 100 Hz IMU data
     * For smart band:
     *   1. Connect, discover Force service
     *   2. Enable notifications → receive 50 Hz force data
     */
    imu_event_t imu_evt;
    force_event_t force_evt;

    while (1) {
        /* Simulated: in production, BLE callbacks would push to queues */
        /* Process any pending BLE events */
        if (g_body_sensors_connected > 0) {
            /* Would receive IMU notifications here */
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* Sub-GHz Task: receives pressure mat frames via SX1262 */
static void subghz_task(void *arg)
{
    ESP_LOGI(TAG, "Sub-GHz task started");
    rs_msg_header_t hdr;
    uint8_t payload[RS_MAX_PAYLOAD];

    while (1) {
        int ret = rs_mesh_recv(&g_mesh, &hdr, payload, sizeof(payload), 5000);
        if (ret > 0 && hdr.msg_type == RS_MSG_PRESSURE_FRAME) {
            /* Parse pressure frame */
            if (ret >= sizeof(rs_pressure_header_t)) {
                rs_pressure_header_t *ph = (rs_pressure_header_t *)payload;
                g_cop_x = ph->cop_x;
                g_cop_y = ph->cop_y;
                g_total_weight_g = ph->total_weight_g;
                g_asymmetry = ph->asymmetry;

                pressure_event_t evt;
                memcpy(&evt.header, ph, sizeof(*ph));
                /* Decompress cells (delta-encoded) */
                if (ret > sizeof(rs_pressure_header_t)) {
                    size_t cell_data = ret - sizeof(rs_pressure_header_t);
                    size_t cells = cell_data / 2;
                    if (cells > 64) cells = 64;
                    memcpy(evt.cells, payload + sizeof(rs_pressure_header_t), cells * 2);
                }
                xQueueSend(g_pressure_queue, &evt, 0);
                g_pressure_mat_connected = 1;
            }
        } else if (ret == -10) {
            /* Join request handled by mesh layer */
        } else if (ret > 0 && hdr.msg_type == RS_MSG_HEARTBEAT) {
            /* Update node table */
            ESP_LOGD(TAG, "Heartbeat from node %d, RSSI %d", hdr.src_id, g_mesh.radio->rssi);
        }
    }
}

/* Sensor Fusion Task: Madgwick AHRS + joint angle derivation */
static void sensor_fusion_task(void *arg)
{
    ESP_LOGI(TAG, "Sensor fusion task started");
    /* Madgwick AHRS filter for each body sensor
     * Quaternion q = [w, x, y, z] from 9-DoF IMU
     * Joint angle = relative orientation between adjacent segments
     * e.g., knee angle = angle between thigh sensor and shin sensor quaternions
     */
    imu_event_t imu_evt;
    float q_sensor[6][4];  /* quaternion per sensor */
    float beta = 0.1f;     /* Madgwick beta parameter */

    /* Initialize quaternions to identity */
    for (int i = 0; i < 6; i++) {
        q_sensor[i][0] = 1.0f;
        q_sensor[i][1] = 0.0f;
        q_sensor[i][2] = 0.0f;
        q_sensor[i][3] = 0.0f;
    }

    while (1) {
        if (xQueueReceive(g_imu_queue, &imu_evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Convert IMU sample to float (mg → m/s², mdps → rad/s) */
            float ax = imu_evt.imu.accel_x * 0.00981f;  /* mg to m/s² */
            float ay = imu_evt.imu.accel_y * 0.00981f;
            float az = imu_evt.imu.accel_z * 0.00981f;
            float gx = imu_evt.imu.gyro_x * 0.01745f;   /* mdps to rad/s */
            float gy = imu_evt.imu.gyro_y * 0.01745f;
            float gz = imu_evt.imu.gyro_z * 0.01745f;

            /* Madgwick AHRS update (simplified — no magnetometer in this stub) */
            float *q = q_sensor[imu_evt.sensor_id % 6];
            float q0 = q[0], q1 = q[1], q2 = q[2], q3 = q[3];

            /* Rate of change of quaternion from gyro */
            float dq0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
            float dq1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
            float dq2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
            float dq3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

            /* Gradient descent correction from accelerometer */
            float recip_norm = 1.0f / sqrtf(ax*ax + ay*ay + az*az + 0.0001f);
            ax *= recip_norm; ay *= recip_norm; az *= recip_norm;

            float s0 = 2.0f * (q1*q3 - q0*q2) - ax;
            float s1 = 2.0f * (q0*q1 + q2*q3) - ay;
            float s2 = 2.0f * (0.5f - q1*q1 - q2*q2) - az;
            s0 = 2.0f * (q1*s0 + q0*s1 + q2*s2);
            s1 = 2.0f * (-q0*s0 + q1*s1 + q2*s2);
            s2 = 2.0f * (q0*s0 - q1*s1 + q2*s2);

            /* Apply feedback step */
            recip_norm = beta / sqrtf(s0*s0 + s1*s1 + s2*s2 + 0.0001f);
            s0 *= recip_norm; s1 *= recip_norm; s2 *= recip_norm;

            /* Integrate */
            q0 += dq0 - s0;
            q1 += dq1 - s1;
            q2 += dq2 - s2;
            q3 += dq3;

            /* Normalize */
            recip_norm = 1.0f / sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3 + 0.0001f);
            q[0] = q0 * recip_norm;
            q[1] = q1 * recip_norm;
            q[2] = q2 * recip_norm;
            q[3] = q3 * recip_norm;

            /* Compute joint angles from adjacent sensor pairs */
            /* Knee: sensor 0 (thigh) vs sensor 1 (shin) */
            if (g_body_sensors_connected >= 2) {
                float *q_thigh = q_sensor[0];
                float *q_shin = q_sensor[1];
                /* Relative quaternion: q_rel = q_thigh^-1 * q_shin */
                float q0i = q_thigh[0], q1i = -q_thigh[1], q2i = -q_thigh[2], q3i = -q_thigh[3];
                float rq0 = q0i*q_shin[0] - q1i*q_shin[1] - q2i*q_shin[2] - q3i*q_shin[3];
                float rq1 = q0i*q_shin[1] + q1i*q_shin[0] + q2i*q_shin[3] - q3i*q_shin[2];
                float rq2 = q0i*q_shin[2] - q1i*q_shin[3] + q2i*q_shin[0] + q3i*q_shin[1];
                float rq3 = q0i*q_shin[3] + q1i*q_shin[2] - q2i*q_shin[1] + q3i*q_shin[0];

                /* Angle from relative quaternion: angle = 2 * acos(|rq0|) */
                float angle = 2.0f * acosf(fabsf(rq0)) * 180.0f / 3.14159265f;
                g_joint_angles[0] = angle;
                if (angle > g_rom_max[0]) g_rom_max[0] = angle;
            }

            /* Shoulder: sensor 2 (upper arm) vs hub IMU (torso reference) */
            if (g_body_sensors_connected >= 3) {
                float *q_arm = q_sensor[2];
                float *q_torso = q_sensor[5]; /* torso sensor or hub IMU */
                float q0i = q_torso[0], q1i = -q_torso[1], q2i = -q_torso[2], q3i = -q_torso[3];
                float rq0 = q0i*q_arm[0] - q1i*q_arm[1] - q2i*q_arm[2] - q3i*q_arm[3];
                float rq1 = q0i*q_arm[1] + q1i*q_arm[0] + q2i*q_arm[3] - q3i*q_arm[2];
                float rq2 = q0i*q_arm[2] - q1i*q_arm[3] + q2i*q_arm[0] + q3i*q_arm[1];
                float rq3 = q0i*q_arm[3] + q1i*q_arm[2] - q2i*q_arm[1] + q3i*q_arm[0];
                float angle = 2.0f * acosf(fabsf(rq0)) * 180.0f / 3.14159265f;
                g_joint_angles[1] = angle;
                if (angle > g_rom_max[1]) g_rom_max[1] = angle;
            }
        }
    }
}

/* Edge ML Task: ExerciseNet + FormNet + RepCount inference */
static void edge_ml_task(void *arg)
{
    ESP_LOGI(TAG, "Edge ML task started");
    /* TFLite-Micro models:
     * ExerciseNet: 1D-CNN, input 1s × 9 features, output 30-class
     * FormNet: Temporal CNN, input 2s × 18 features (joint angles), output form score + deviation
     * RepCount: Peak detection state machine on joint angle + force
     *
     * In production: load .tflite models from flash, allocate tensors,
     * run inference every 500ms for ExerciseNet, every 200ms for FormNet,
     * continuously for RepCount.
     */

    /* Sliding window buffers */
    float imu_window[100][9];  /* 1 second at 100 Hz */
    float angle_window[200][18]; /* 2 seconds at 100 Hz */
    int imu_idx = 0;
    int angle_idx = 0;

    /* Rep counting state machine */
    enum rep_state {
        REP_IDLE,
        REP_CONCENTRIC,
        REP_PEAK,
        REP_ECCENTRIC,
    };
    enum rep_state rep_state = REP_IDLE;
    float rep_threshold = 30.0f;  /* degrees, exercise-specific */
    float rep_hysteresis = 10.0f;
    float prev_angle = 0.0f;

    while (1) {
        if (!g_session_active) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Fill IMU window from queue (non-blocking) */
        imu_event_t imu_evt;
        while (xQueueReceive(g_imu_queue, &imu_evt, 0) == pdTRUE) {
            if (imu_evt.sensor_id == 0) {  /* primary sensor for exercise ID */
                imu_window[imu_idx][0] = imu_evt.imu.accel_x;
                imu_window[imu_idx][1] = imu_evt.imu.accel_y;
                imu_window[imu_idx][2] = imu_evt.imu.accel_z;
                imu_window[imu_idx][3] = imu_evt.imu.gyro_x;
                imu_window[imu_idx][4] = imu_evt.imu.gyro_y;
                imu_window[imu_idx][5] = imu_evt.imu.gyro_z;
                /* Mag would be added from separate notification */
                imu_idx = (imu_idx + 1) % 100;
            }
        }

        /* Run ExerciseNet inference every 500ms */
        static uint32_t last_ex_time = 0;
        if (esp_timer_get_time() / 1000 - last_ex_time > 500) {
            /* tflite::MicroInterpreter invoke() with imu_window */
            /* g_exercise_id = model output argmax */
            /* g_exercise_confidence = softmax max */
            /* Placeholder: keep current exercise */
            last_ex_time = esp_timer_get_time() / 1000;
        }

        /* Run FormNet inference every 200ms */
        static uint32_t last_form_time = 0;
        if (esp_timer_get_time() / 1000 - last_form_time > 200) {
            /* Fill angle window from g_joint_angles */
            for (int i = 0; i < 6; i++) {
                angle_window[angle_idx][i] = g_joint_angles[i];
                angle_window[angle_idx][i + 6] = g_rom_max[i];
            }
            angle_idx = (angle_idx + 1) % 200;

            /* tflite::MicroInterpreter invoke() with angle_window */
            /* g_form_score = model output[0] (0-100) */
            /* g_form_deviation = model output[1] (deviation type) */
            /* Placeholder: keep defaults */
            last_form_time = esp_timer_get_time() / 1000;
        }

        /* Rep counting state machine (continuous) */
        if (g_body_sensors_connected >= 2) {
            float angle = g_joint_angles[0];  /* primary joint */
            switch (rep_state) {
                case REP_IDLE:
                    if (angle > rep_threshold) {
                        rep_state = REP_CONCENTRIC;
                    }
                    break;
                case REP_CONCENTRIC:
                    if (angle > prev_angle && angle > rep_threshold + rep_hysteresis) {
                        rep_state = REP_PEAK;
                    }
                    break;
                case REP_PEAK:
                    if (angle < prev_angle) {
                        rep_state = REP_ECCENTRIC;
                    }
                    break;
                case REP_ECCENTRIC:
                    if (angle < rep_threshold) {
                        g_rep_count++;
                        ESP_LOGI(TAG, "Rep %d completed, form score %d", g_rep_count, g_form_score);

                        /* Check form score for feedback */
                        if (g_form_score < 60 && g_form_deviation != RS_DEV_NONE) {
                            /* Trigger form correction feedback */
                            const char *deviation_msgs[] = {
                                "", "Watch your knee alignment",
                                "Level your hips", "Reduce trunk lean",
                                "Increase range of motion",
                                "Slow down", "Correct asymmetry"
                            };
                            uint8_t msg_idx = g_form_deviation;
                            if (msg_idx <= 6) {
                                /* Queue audio feedback */
                                xQueueSend(g_feedback_queue, &msg_idx, 0);
                            }
                        }

                        /* Check if target reps reached */
                        if (g_rep_count >= g_target_reps) {
                            ESP_LOGI(TAG, "Target reps reached! Exercise complete.");
                            /* Trigger completion feedback */
                        }
                        rep_state = REP_IDLE;
                    }
                    break;
            }
            prev_angle = angle;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* 100 Hz loop */
    }
}

/* Feedback Task: audio coaching + haptic + display updates */
static void feedback_task(void *arg)
{
    ESP_LOGI(TAG, "Feedback task started");
    uint8_t feedback_cmd;

    while (1) {
        if (xQueueReceive(g_feedback_queue, &feedback_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Audio feedback via MAX98357A I²S speaker
             * Pre-recorded coaching messages indexed by feedback_cmd
             * Haptic pattern via DRV2605L
             */
            const char *messages[] = {
                "Great form!", "Watch your knee alignment", "Level your hips",
                "Reduce trunk lean", "Increase range of motion", "Slow down",
                "Correct asymmetry", "Excellent!", "Two more reps", "Done!",
            };
            if (feedback_cmd < 10) {
                ESP_LOGI(TAG, "Audio feedback: %s", messages[feedback_cmd]);
                /* I²S write: play audio clip */
                /* DRV2605L: trigger haptic pattern */
            }
        }

        /* Update TFT display (every 200ms) */
        static uint32_t last_display = 0;
        if (esp_timer_get_time() / 1000 - last_display > 200 && g_session_active) {
            /* Update display:
             * - Current exercise name
             * - Rep count: X / Y
             * - Form score: XX/100
             * - Joint angle: XX°
             * - Force: XX kg (if band connected)
             * - Weight distribution (if mat connected)
             */
            last_display = esp_timer_get_time() / 1000;
        }
    }
}

/* Cloud Task: Wi-Fi/MQTT telemetry upload + OTA */
static void cloud_task(void *arg)
{
    ESP_LOGI(TAG, "Cloud task started");
    /* Wi-Fi connection + MQTT client
     * Topics:
     *   rehab-sync/telemetry/{hub_id} — sensor data every 1s
     *   rehab-sync/session/{hub_id} — session data every 10s
     *   rehab-sync/alerts/{hub_id} — alerts (event-driven)
     *   rehab-sync/commands/{hub_id} — exercise plan updates, OTA
     */

    while (1) {
        if (g_session_active) {
            /* Build telemetry JSON */
            char telemetry[512];
            snprintf(telemetry, sizeof(telemetry),
                "{\"ts\":%lu,\"session\":%u,\"exercise\":%u,\"reps\":%u,\"form\":%u,"
                "\"dev\":%u,\"knee_angle\":%.1f,\"shoulder_angle\":%.1f,"
                "\"force_mg\":%ld,\"weight_g\":%u,\"asymmetry\":%u,"
                "\"sensors\":%u,\"band\":%u,\"mat\":%u}",
                (unsigned long)(esp_timer_get_time() / 1000),
                g_session_active, g_current_exercise, g_rep_count, g_form_score,
                g_form_deviation, g_joint_angles[0], g_joint_angles[1],
                (long)g_current_force_mg, g_total_weight_g, g_asymmetry,
                g_body_sensors_connected, g_smart_band_connected, g_pressure_mat_connected);

            /* esp_mqtt_client_publish(g_mqtt_client, topic, telemetry, 0, 1, 0) */
            ESP_LOGD(TAG, "Telemetry: %s", telemetry);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* OTA Task: distribute firmware updates to body sensors via BLE GATT */
static void ota_task(void *arg)
{
    ESP_LOGI(TAG, "OTA task started");
    /* When cloud pushes firmware update:
     * 1. Download image to flash
     * 2. For body sensors: push via BLE GATT OTA characteristic in 128-byte blocks
     * 3. For pressure mat: push via Sub-GHz OTA blocks
     * 4. Verify checksum, trigger reboot on target node
     */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        /* Check for pending OTA commands */
    }
}

/* === App main === */
void app_main(void)
{
    ESP_LOGI(TAG, "RehabSync Hub starting...");

    /* NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Create queues */
    g_imu_queue = xQueueCreate(256, sizeof(imu_event_t));
    g_force_queue = xQueueCreate(128, sizeof(force_event_t));
    g_pressure_queue = xQueueCreate(32, sizeof(pressure_event_t));
    g_feedback_queue = xQueueCreate(16, sizeof(uint8_t));

    /* Create mutex */
    g_radio_mutex = xSemaphoreCreateMutex();

    /* Initialize peripherals */
    spi_init();
    i2c_init();

    /* Initialize SX1262 radio */
    static sx1262_t radio;
    static sx1262_config_t radio_cfg = {
        .frequency = RS_BAND_868MHZ,
        .spreading_factor = RS_SF,
        .bandwidth = RS_BW,
        .coding_rate = 1,
        .tx_power_dbm = RS_TX_POWER,
        .preamble_len = RS_PREAMBLE_LEN,
    };
    sx1262_init(&radio, &radio_cfg);

    /* Initialize mesh as coordinator */
    rs_mesh_init(&g_mesh, 0x01, RS_NODE_HUB, true, &radio);

    /* Initialize Wi-Fi */
    /* esp_netif_init(); esp_event_loop_create_default(); */
    /* wifi_init(); mqtt_init(); */

    /* Create tasks */
    xTaskCreate(ble_central_task, "ble", 8192, NULL, 5, NULL);
    xTaskCreate(subghz_task, "subghz", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_fusion_task, "fusion", 8192, NULL, 6, NULL);
    xTaskCreate(edge_ml_task, "ml", 12288, NULL, 5, NULL);
    xTaskCreate(feedback_task, "feedback", 4096, NULL, 3, NULL);
    xTaskCreate(cloud_task, "cloud", 8192, NULL, 3, NULL);
    xTaskCreate(ota_task, "ota", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "RehabSync Hub ready. Waiting for sensors...");
}
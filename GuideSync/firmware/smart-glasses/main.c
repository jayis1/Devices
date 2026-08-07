/*
 * GuideSync — Smart Glasses Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Smart Glasses are the primary vision node. They continuously
 * capture scene images (OV5640), run SceneNet (YOLOv8-nano) for
 * object/obstacle detection, read the VL53L5CX 8x8 ToF array for
 * depth-verified proximity (ObstacleNet), detect crosswalks and
 * pedestrian signals (CrosswalkNet), and output audio descriptions
 * via bone conduction transducers. Voice commands ("read text",
 * "describe scene", "where am I") are captured via I²S MEMS mic.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/ble_mesh.h"
#include "../common/ble_beacon.h"
#include "../common/config.h"

static const char *TAG = "GuideSync-Glasses";

/* === Global state === */
static gs_ble_ctx_t g_ble;
static QueueHandle_t g_scene_queue;
static QueueHandle_t g_audio_queue;

/* Scene state */
static uint8_t g_obstacle_class = 0;
static uint8_t g_obstacle_dist_dm = 255;
static uint8_t g_obstacle_dir = 0;
static uint8_t g_crosswalk_detected = 0;
static uint8_t g_signal_state = 0;
static uint8_t g_countdown_sec = 0;
static uint8_t g_tof_min_dist_dm = 255;
static uint8_t g_tof_hazard_flag = 0;
static uint16_t g_scenenet_ms = 0;
static uint16_t g_crosswalknet_ms = 0;
static uint16_t g_step_count = 0;

/* Camera frame buffer */
static camera_config_t g_camera_config;
static bool g_camera_ready = false;

/* === BLE Interface === */
static void ble_init(void) { /* NimBLE peripheral init */ }
static void ble_advert_start(void) { /* esp_ble_gap_start_advertising */ }
static void ble_advert_stop(void) { /* esp_ble_gap_stop_advertising */ }
static void ble_scan_start(void) { }
static void ble_scan_stop(void) { }
static int ble_connect(uint8_t *peer_addr) { (void)peer_addr; return 0; }
static int ble_send(const uint8_t *data, uint8_t len)
{
    ESP_LOGI(TAG, "BLE TX %d bytes", len);
    return len;
}
static int ble_recv(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms)
{
    vTaskDelay(pdMS_TO_TICKS(timeout_ms > 100 ? 100 : timeout_ms));
    return 0;
}
static void ble_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void ble_disconnect(void) { }

static const gs_ble_interface_t g_ble_iface = {
    .init = ble_init,
    .advert_start = ble_advert_start,
    .advert_stop = ble_advert_stop,
    .scan_start = ble_scan_start,
    .scan_stop = ble_scan_stop,
    .connect = ble_connect,
    .send = ble_send,
    .recv = ble_recv,
    .delay_ms = ble_delay_ms,
    .disconnect = ble_disconnect,
};

/* === Beacon Scanner Interface === */
static void beacon_scan_start(void) { /* esp_ble_gap_start_scanning */ }
static void beacon_scan_stop(void) { /* esp_ble_gap_stop_scanning */ }
static int beacon_get_results(gs_beacon_result_t *results, uint8_t max_count)
{
    /* Stub: return 0 beacons */
    (void)results; (void)max_count;
    return 0;
}
static void beacon_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static const gs_beacon_scan_if_t g_beacon_if = {
    .scan_start = beacon_scan_start,
    .scan_stop = beacon_scan_stop,
    .scan_get_results = beacon_get_results,
    .delay_ms = beacon_delay_ms,
};

/* === Camera (OV5640) === */
static int camera_init(void)
{
    g_camera_config.ledc_channel = LEDC_CHANNEL_0;
    g_camera_config.ledc_timer = LEDC_TIMER_0;
    g_camera_config.pin_d0 = GLASSES_GPIO_CAM_D0;
    g_camera_config.pin_d1 = GLASSES_GPIO_CAM_D1;
    g_camera_config.pin_d2 = GLASSES_GPIO_CAM_D2;
    g_camera_config.pin_d3 = GLASSES_GPIO_CAM_D3;
    g_camera_config.pin_d4 = GLASSES_GPIO_CAM_D4;
    g_camera_config.pin_d5 = GLASSES_GPIO_CAM_D5;
    g_camera_config.pin_d6 = GLASSES_GPIO_CAM_D6;
    g_camera_config.pin_d7 = GLASSES_GPIO_CAM_D7;
    g_camera_config.pin_vsync = GLASSES_GPIO_CAM_VSYNC;
    g_camera_config.pin_href = GLASSES_GPIO_CAM_HREF;
    g_camera_config.pin_pclk = GLASSES_GPIO_CAM_PCLK;
    g_camera_config.pin_xclk = GLASSES_GPIO_CAM_XCLK;
    g_camera_config.pin_sccb_sda = GLASSES_GPIO_CAM_SIOD;
    g_camera_config.pin_sccb_scl = GLASSES_GPIO_CAM_SIOC;
    g_camera_config.xclk_freq_hz = 20000000;
    g_camera_config.frame_size = FRAMESIZE_QVGA; /* 320x240 → resize to 320x320 */
    g_camera_config.pixel_format = PIXFORMAT_RGB565;
    g_camera_config.fb_count = 2;
    g_camera_config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&g_camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return -1;
    }
    g_camera_ready = true;
    ESP_LOGI(TAG, "OV5640 camera initialized (320x240 RGB565)");
    return 0;
}

/* === VL53L5CX ToF Array === */
static int tof_init(void)
{
    /* I²C init at GLASSES_GPIO_TOF_SDA/SCL, address 0x29 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GLASSES_GPIO_TOF_SDA,
        .scl_io_num = GLASSES_GPIO_TOF_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_1, &conf);
    i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
    /* VL53L5CX init: set 8x8 resolution, 15 Hz, 4 m max range */
    ESP_LOGI(TAG, "VL53L5CX ToF initialized (8x8, 15 Hz, 4 m)");
    return 0;
}

/* Read VL53L5CX 8x8 depth grid (64 zones) */
static void tof_read_grid(uint8_t *depth_dm, uint8_t *min_dm, uint8_t *valid_zones)
{
    /* Production: read 64-zone data via I²C from VL53L5CX */
    /* Stub: fill with "clear" values */
    for (int i = 0; i < 64; i++) {
        depth_dm[i] = 255; /* No obstacle */
    }
    *min_dm = 255;
    *valid_zones = 64;
}

/* === SceneNet Inference (YOLOv8-nano) ===
 * In production: TFLite-Micro interpreter with int8 quantized model.
 * Input: 320x320 RGB, Output: bounding boxes + class labels.
 * Here: stub that simulates detection.
 */
static void run_scenenet(uint8_t *obj_class, uint8_t *obj_dist_dm,
                         uint8_t *obj_dir, uint8_t *obj_count)
{
    if (!g_camera_ready) {
        *obj_class = 0; *obj_dist_dm = 255; *obj_dir = 0; *obj_count = 0;
        return;
    }

    /* Capture frame */
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        *obj_class = 0; *obj_dist_dm = 255; *obj_dir = 0; *obj_count = 0;
        return;
    }

    /* Production: preprocess 320x240 RGB565 → 320x320 RGB888 → TFLite input tensor
     * Run YOLOv8-nano inference → NMS → extract detections
     */
    g_scenenet_ms = 280; /* Simulated inference time */

    /* Stub: no objects detected */
    *obj_class = 0;
    *obj_dist_dm = 255;
    *obj_dir = 0;
    *obj_count = 0;

    esp_camera_fb_return(fb);
}

/* === ObstacleNet Inference (ToF depth grid) ===
 * 1D-CNN over 64-zone depth vector → 6-class hazard classification
 */
static void run_obstaclenet(const uint8_t *depth_dm, uint8_t *hazard_class,
                            uint8_t *min_dist_dm)
{
    uint8_t min_dm = 255;
    for (int i = 0; i < 64; i++) {
        if (depth_dm[i] < min_dm && depth_dm[i] > 0) {
            min_dm = depth_dm[i];
        }
    }
    *min_dist_dm = min_dm;

    /* Production: TFLite-Micro inference on 64-element depth vector */
    if (min_dm <= TOF_CRITICAL_DIST_DM) {
        /* Determine direction from zone position */
        int zone = 0;
        for (int i = 0; i < 64; i++) {
            if (depth_dm[i] == min_dm) { zone = i; break; }
        }
        if (zone % 8 < 3) *hazard_class = 3;      /* Left */
        else if (zone % 8 > 4) *hazard_class = 3;  /* Right */
        else if (zone < 32) *hazard_class = 1;     /* Low */
        else *hazard_class = 2;                     /* High */
    } else if (min_dm <= TOF_HAZARD_DIST_DM) {
        *hazard_class = 4; /* Approaching */
    } else {
        *hazard_class = 5; /* Open space */
    }
}

/* === CrosswalkNet Inference ===
 * MobileNetV3-small → crosswalk + signal state classification
 */
static void run_crosswalknet(uint8_t *crosswalk, uint8_t *signal_state,
                             uint8_t *countdown)
{
    /* Production: crop lower half of camera frame → 224x224 → TFLite inference */
    g_crosswalknet_ms = 75;
    *crosswalk = 0;
    *signal_state = 0;
    *countdown = 0;
}

/* === Bone Conduction Audio Output ===
 * I²S → MAX98357A → bone conduction transducers
 * Production: TTS (esp_tts or picoTTS) generates speech from text,
 * outputs via I²S to amplifier.
 */
static void speak_text(const char *text)
{
    ESP_LOGI(TAG, "SPEAK: \"%s\"", text);
    /* Production: TTS synthesis → I²S output to MAX98357A */
}

/* Describe scene in natural language for bone conduction output */
static void describe_scene(uint8_t obj_class, uint8_t obj_dist_dm,
                           uint8_t obj_dir, uint8_t hazard_class,
                           uint8_t tof_min_dm, uint8_t crosswalk,
                           uint8_t signal_state)
{
    char desc[256];

    /* Priority: safety-critical first */
    if (tof_min_dm <= TOF_CRITICAL_DIST_DM) {
        const char *direction = "ahead";
        if (hazard_class == 3) direction = "to your left";
        else if (hazard_class == 2) direction = "at head level";
        else if (hazard_class == 1) direction = "low, step over";
        snprintf(desc, sizeof(desc), "Obstacle %s, %.1f meters", direction, tof_min_dm / 10.0);
        speak_text(desc);
        return;
    }

    if (crosswalk && signal_state == 1) {
        speak_text("Crosswalk ahead, walk signal on");
        return;
    }
    if (crosswalk && signal_state == 2) {
        speak_text("Crosswalk ahead, wait, do not walk");
        return;
    }

    /* Non-critical scene description */
    if (obj_dist_dm < 255) {
        snprintf(desc, sizeof(desc), "%s at %.1f meters",
                 "object", obj_dist_dm / 10.0);
        speak_text(desc);
    }
}

/* === ICM-42688 IMU (head tracking + step counting) === */
static void imu_read(int8_t *pitch, int8_t *roll, int8_t *yaw, int8_t *temp)
{
    /* Production: read ICM-42688 via I²C, apply Madgwick filter for orientation */
    *pitch = 0; *roll = 0; *yaw = 0; *temp = 25;
}

static void imu_step_detect(void)
{
    /* Production: accelerometer peak detection for step counting */
    g_step_count++;
}

/* === Voice Command (I²S mic) ===
 * Production: keyword detection (Picovoice Porcupine or custom KWS CNN)
 * Keywords: "read text", "describe scene", "where am I", "stop"
 */
static void voice_command_task(void *arg)
{
    ESP_LOGI(TAG, "Voice command task started (I²S mic)");
    /* I²S config for ICS-43434 mic at GLASSES_GPIO_MIC_BCLK/LRCLK/DATA */
    while (1) {
        /* Production: read I²S audio → KWS inference → trigger action */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* === Scene Processing Task === */
static void scene_task(void *arg)
{
    ESP_LOGI(TAG, "Scene processing task started (SceneNet + ObstacleNet + CrosswalkNet)");

    uint8_t depth_grid[64];
    uint8_t tof_min_dm, tof_valid;
    uint8_t obj_class, obj_dist_dm, obj_dir, obj_count;
    uint8_t hazard_class;
    uint8_t crosswalk, signal_state, countdown;

    uint8_t fps = GLASSES_SCENE_FPS_CONT;
    uint32_t idle_timer = 0;

    while (1) {
        /* Read ToF grid */
        tof_read_grid(depth_grid, &tof_min_dm, &tof_valid);

        /* Run ObstacleNet on ToF data */
        run_obstaclenet(depth_grid, &hazard_class, &tof_min_dm);

        /* Run SceneNet on camera */
        run_scenenet(&obj_class, &obj_dist_dm, &obj_dir, &obj_count);

        /* Run CrosswalkNet */
        run_crosswalknet(&crosswalk, &signal_state, &countdown);

        /* Update global state */
        g_obstacle_class = obj_class;
        g_obstacle_dist_dm = obj_dist_dm;
        g_obstacle_dir = obj_dir;
        g_tof_min_dist_dm = tof_min_dm;
        g_tof_hazard_flag = hazard_class;
        g_crosswalk_detected = crosswalk;
        g_signal_state = signal_state;
        g_countdown_sec = countdown;

        /* Determine FPS: faster when obstacle detected */
        if (tof_min_dm <= TOF_HAZARD_DIST_DM || obj_dist_dm < SCENE_OBSTACLE_DIST_DM) {
            fps = GLASSES_SCENE_FPS_ACTIVE;
            idle_timer = 0;
        } else {
            idle_timer++;
            if (idle_timer > GLASSES_IDLE_TIMEOUT_S * fps) {
                fps = GLASSES_SCENE_FPS_CONT;
            }
        }

        /* Describe scene via bone conduction */
        describe_scene(obj_class, obj_dist_dm, obj_dir, hazard_class,
                       tof_min_dm, crosswalk, signal_state);

        /* Send telemetry to hub every ~5 seconds */
        static uint32_t telem_counter = 0;
        if (++telem_counter >= (uint32_t)(5 * fps)) {
            int8_t pitch, roll, yaw, imu_temp;
            imu_read(&pitch, &roll, &yaw, &imu_temp);

            uint8_t battery_v = 380; /* Stub: 3.80V */

            gs_message_t telem;
            gs_build_glasses_telem(&telem, g_ble.node_id, g_ble.msg_seq++,
                battery_v, pitch, roll, yaw,
                obj_class, obj_dist_dm, obj_dir,
                obj_count, obj_class, obj_dist_dm,
                crosswalk, signal_state, countdown,
                tof_min_dm, hazard_class,
                50, 0, /* audio_vol, bone_conduction_active */
                g_step_count, imu_temp,
                g_scenenet_ms, g_crosswalknet_ms,
                (uint16_t)esp_get_free_heap_size(), -60, /* rssi */
                (uint16_t)(esp_timer_get_time() / 60000000ULL), /* uptime_min */
                tof_valid);

            if (g_ble.joined) {
                gs_ble_send(&g_ble, &telem);
            }
            telem_counter = 0;
        }

        /* Send alert if critical obstacle */
        if (tof_min_dm <= TOF_CRITICAL_DIST_DM) {
            gs_message_t alert;
            uint8_t alert_data[3] = {obj_class, tof_min_dm, hazard_class};
            gs_build_alert(&alert, g_ble.node_id, g_ble.msg_seq++,
                          GS_ALERT_OBSTACLE_CRIT, GS_SEV_CRITICAL,
                          alert_data, 3);
            if (g_ble.joined) {
                gs_ble_send(&g_ble, &alert);
            }
        }

        /* Send crosswalk alert */
        if (crosswalk && (signal_state == 1 || signal_state == 2)) {
            gs_message_t alert;
            uint8_t alert_data[2] = {signal_state, countdown};
            uint8_t alert_type = (signal_state == 1) ?
                GS_ALERT_CROSSWALK_WALK : GS_ALERT_CROSSWALK_DONT;
            gs_build_alert(&alert, g_ble.node_id, g_ble.msg_seq++,
                          alert_type, GS_SEV_WARNING, alert_data, 2);
            if (g_ble.joined) {
                gs_ble_send(&g_ble, &alert);
            }
        }

        /* Frame delay */
        vTaskDelay(pdMS_TO_TICKS(1000 / fps));
    }
}

/* === Beacon Scan Task === */
static void beacon_task(void *arg)
{
    gs_beacon_scanner_init(&g_beacon_if);
    ESP_LOGI(TAG, "Beacon scan task started (NavNet)");

    while (1) {
        if (g_ble.joined) {
            gs_beacon_scan_t scan;
            if (gs_beacon_scan(&scan) == 0) {
                /* Build beacon scan message and send to hub */
                uint16_t uuids[GS_BEACON_MAX_PER_SCAN];
                int8_t rssi_vals[GS_BEACON_MAX_PER_SCAN];
                uint8_t dists[GS_BEACON_MAX_PER_SCAN];

                for (uint8_t i = 0; i < scan.count; i++) {
                    uuids[i] = scan.results[i].uuid_short;
                    rssi_vals[i] = scan.results[i].rssi;
                    dists[i] = scan.results[i].distance_dm;
                }

                gs_message_t scan_msg;
                gs_build_beacon_scan(&scan_msg, g_ble.node_id, g_ble.msg_seq++,
                                    scan.count, uuids, rssi_vals, dists);
                gs_ble_send(&g_ble, &scan_msg);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(3000)); /* Scan every 3 seconds */
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "GuideSync Smart Glasses starting...");

    nvs_flash_init();

    /* Init subsystems */
    camera_init();
    tof_init();

    /* Queues */
    g_scene_queue = xQueueCreate(8, sizeof(gs_message_t));
    g_audio_queue = xQueueCreate(4, 256);

    /* BLE init + join */
    gs_ble_init(&g_ble, GS_NODE_GLASSES, &g_ble_iface);
    /* gs_ble_join(&g_ble); — production: auto-join on boot */

    /* Tasks */
    xTaskCreate(scene_task, "scene", 16384, NULL, 5, NULL);
    xTaskCreate(voice_command_task, "voice", 8192, NULL, 3, NULL);
    xTaskCreate(beacon_task, "beacon", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "GuideSync Smart Glasses ready.");
}
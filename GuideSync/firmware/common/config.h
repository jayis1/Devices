/*
 * GuideSync — Configuration Constants
 * Pin assignments, BLE UUIDs, network parameters, calibration defaults
 */
#ifndef GUIDESYNC_CONFIG_H
#define GUIDESYNC_CONFIG_H

/* === BLE Network Parameters === */
#define GS_BLE_SERVICE_UUID       0x47, 0x53, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BLE_CHAR_TELEMETRY     0x47, 0x53, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BLE_CHAR_COMMAND       0x47, 0x53, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BLE_CHAR_NAV           0x47, 0x53, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BLE_CHAR_ALERT         0x47, 0x53, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BLE_CHAR_OTA           0x47, 0x53, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00
#define GS_BEACON_UUID_PREFIX     0x47, 0x53, 0xBE, 0xAC  /* GS beacon prefix */

/* Connection intervals (ms) */
#define GS_BLE_CONN_INTERVAL_GLASSES   50    /* High data: glasses */
#define GS_BLE_CONN_INTERVAL_CANE      200   /* Medium: cane */
#define GS_BLE_CONN_INTERVAL_BAND      200   /* Medium: band */
#define GS_BEACON_ADV_INTERVAL_MS      500   /* Beacon advertising */

#define GS_MAX_NODES              35   /* 3 peripherals + 32 beacons */
#define GS_MAX_BEACONS            32
#define GS_AES_KEY_LEN            16

/* === Hub (ESP32-S3) Pin Map === */
#define HUB_GPIO_BME_SDA         4
#define HUB_GPIO_BME_SCL         5
#define HUB_GPIO_RTC_SDA         6
#define HUB_GPIO_RTC_SCL         7
#define HUB_GPIO_SD_MOSI         8
#define HUB_GPIO_SD_MISO         9
#define HUB_GPIO_SD_SCK         10
#define HUB_GPIO_SD_CS          11
#define HUB_GPIO_LED            12
#define HUB_GPIO_BUZZER         13
#define HUB_GPIO_CELL_TX        14
#define HUB_GPIO_CELL_RX        15
#define HUB_GPIO_CELL_PWR       16
#define HUB_GPIO_VBAT           17
#define HUB_GPIO_USB_PWR        18
#define HUB_GPIO_UART_TX        43
#define HUB_GPIO_UART_RX        44

/* === Smart Glasses (ESP32-S3) Pin Map === */
/* OV5640 camera parallel bus */
#define GLASSES_GPIO_CAM_D0      11
#define GLASSES_GPIO_CAM_D1      10
#define GLASSES_GPIO_CAM_D2       9
#define GLASSES_GPIO_CAM_D3       8
#define GLASSES_GPIO_CAM_D4       7
#define GLASSES_GPIO_CAM_D5       6
#define GLASSES_GPIO_CAM_D6       5
#define GLASSES_GPIO_CAM_D7       4
#define GLASSES_GPIO_CAM_VSYNC   12
#define GLASSES_GPIO_CAM_HREF    13
#define GLASSES_GPIO_CAM_PCLK    14
#define GLASSES_GPIO_CAM_XCLK    15
#define GLASSES_GPIO_CAM_SIOC    16
#define GLASSES_GPIO_CAM_SIOD    17
/* VL53L5CX ToF + ICM-42688 IMU (I²C) */
#define GLASSES_GPIO_TOF_SDA     18
#define GLASSES_GPIO_TOF_SCL     19
#define GLASSES_GPIO_IMU_SDA     20
#define GLASSES_GPIO_IMU_SCL     21
/* I²S mic (voice commands) */
#define GLASSES_GPIO_MIC_BCLK    22
#define GLASSES_GPIO_MIC_LRCLK   23
#define GLASSES_GPIO_MIC_DATA    24
/* I²S amp (bone conduction) */
#define GLASSES_GPIO_AMP_BCLK    25
#define GLASSES_GPIO_AMP_LRCLK   26
#define GLASSES_GPIO_AMP_DATA    27
#define GLASSES_GPIO_VBAT        28
#define GLASSES_GPIO_LED         29
#define GLASSES_GPIO_USB_PWR     30

/* === Smart Cane (nRF52840) Pin Map === */
#define CANE_GPIO_TOF_SCL        2   /* P0.02 */
#define CANE_GPIO_TOF_SDA        3   /* P0.03 */
#define CANE_GPIO_IMU_SCL        4   /* P0.04 */
#define CANE_GPIO_IMU_SDA        5   /* P0.05 */
#define CANE_GPIO_HAPTIC_SCL     6   /* P0.06 */
#define CANE_GPIO_HAPTIC_SDA     7   /* P0.07 */
#define CANE_GPIO_US_TRIG        8   /* P0.08 */
#define CANE_GPIO_US_ECHO        9   /* P0.09 */
#define CANE_GPIO_TOF_INT       10   /* P0.10 */
#define CANE_GPIO_VBAT          11   /* P0.11 / AIN11 */
#define CANE_GPIO_USB_PWR       12   /* P0.12 */
#define CANE_GPIO_LED           13   /* P0.13 */
#define CANE_GPIO_IMU_INT       14   /* P0.14 */
#define CANE_GPIO_MOT_EN        15   /* P0.15 */

/* === Haptic Band (nRF52840) Pin Map === */
#define BAND_GPIO_IMU_SCL        2   /* P0.02 */
#define BAND_GPIO_IMU_SDA        3   /* P0.03 */
#define BAND_GPIO_HAPTIC_SCL     4   /* P0.04 */
#define BAND_GPIO_HAPTIC_SDA     5   /* P0.05 */
#define BAND_GPIO_SOS_BTN        6   /* P0.06 */
#define BAND_GPIO_IMU_INT1       7   /* P0.07 */
#define BAND_GPIO_IMU_INT2       8   /* P0.08 */
#define BAND_GPIO_VBAT           9   /* P0.09 / AIN9 */
#define BAND_GPIO_MOT_EN        10   /* P0.10 */
#define BAND_GPIO_LED           11   /* P0.11 */
#define BAND_GPIO_USB_PWR       12   /* P0.12 */

/* === Nav Beacon (nRF52840) Pin Map === */
#define BEACON_GPIO_LED          2   /* P0.02 */
#define BEACON_GPIO_REED         3   /* P0.03 */
#define BEACON_GPIO_VBAT         4   /* P0.04 / AIN4 */

/* === Node Types === */
#define GS_NODE_HUB              0x00
#define GS_NODE_GLASSES          0x01
#define GS_NODE_CANE             0x02
#define GS_NODE_BAND             0x03
#define GS_NODE_BEACON           0x04

#define GS_HUB_NODE_ID           0x00
#define GS_BROADCAST             0xFF

/* === Sampling Intervals === */
#define GLASSES_SCENE_FPS_CONT     2    /* 2 fps continuous */
#define GLASSES_SCENE_FPS_ACTIVE   5    /* 5 fps when obstacle detected */
#define GLASSES_IDLE_TIMEOUT_S     300  /* 5 min no obstacle → idle */
#define CANE_SAMPLE_INTERVAL_MS    100  /* 10 Hz ultrasonic */
#define CANE_TOF_INTERVAL_MS       200  /* 5 Hz downward ToF */
#define BAND_IMU_SAMPLE_HZ         200  /* 200 Hz for FallNet */
#define BAND_HAPTIC_INTERVAL_MS    5000 /* Nav haptic every 5s */
#define BEACON_ADV_INTERVAL_MS     500  /* 500 ms advertising */
#define HEARTBEAT_INTERVAL_S       60   /* 1 minute */

/* === SceneNet Detection Thresholds === */
#define SCENE_CONFIDENCE_PCT       35   /* YOLOv8-nano conf threshold */
#define SCENE_OBSTACLE_DIST_DM     20   /* 2 m → obstacle alert */
#define SCENE_OBSTACLE_CRITICAL_DM 10   /* 1 m → critical alert */

/* === ObstacleNet ToF Thresholds === */
#define TOF_HAZARD_DIST_DM         20   /* 2 m → hazard zone */
#define TOF_CRITICAL_DIST_DM       10   /* 1 m → critical */
#define TOF_VALID_ZONE_MIN_CM      30   /* 30 cm min valid reading */

/* === CrosswalkNet Thresholds === */
#define CROSSWALK_CONFIDENCE_PCT   90   /* Must be >90% to announce walk */
#define CROSSWALK_SCAN_INTERVAL_S  2    /* Check every 2s */

/* === Fall Detection Thresholds === */
#define FALL_FREEFALL_THRESH_MG    500  /* <0.5g free-fall trigger */
#define FALL_IMPACT_THRESH_MG      2500 /* >2.5g impact trigger */
#define FALL_POST_STILLNESS_S      3    /* 3s stillness post-impact */
#define FALL_CANCEL_WINDOW_S       30   /* 30s to cancel false alarm */
#define FALLNET_INFERENCE_HZ       1    /* Run FallNet 1×/sec on 2s window */

/* === SOS === */
#define SOS_PRESS_DURATION_MS      3000 /* 3s long-press */
#define SOS_CANCEL_PRESSES         3    /* 3× rapid press to cancel */
#define SOS_CANCEL_WINDOW_S        60   /* 60s cancel window */

/* === Navigation Haptic Patterns (DRV2605L waveform IDs) === */
#define HAPTIC_NAV_CONTINUE_SEQ    {1, 0}                          /* Sharp click */
#define HAPTIC_NAV_LEFT_SEQ        {2, 0, 2, 0}                    /* Double strong click */
#define HAPTIC_NAV_RIGHT_SEQ       {1, 0, 1, 0, 1, 0}             /* Triple sharp click */
#define HAPTIC_NAV_STOP_SEQ        {14}                            /* Long hum 500ms */
#define HAPTIC_NAV_ARRIVE_SEQ      {8, 10, 12}                    /* Ascending */
#define HAPTIC_FALL_SEQ            {14, 0, 14, 0, 14, 0, 14, 0, 14} /* Urgent rumble */
#define HAPTIC_SOS_CONFIRM_SEQ     {12, 10, 8}                    /* Descending */
#define HAPTIC_LOW_BAT_SEQ         {8}                             /* Soft bump */

/* === Battery Thresholds (x0.01V) === */
#define BAT_LOW_MV                 330   /* 3.30V → low battery */
#define BAT_CRIT_MV                300   /* 3.00V → critical */
#define BAT_FULL_MV                420   /* 4.20V → full (LiPo) */
#define BAT_CR2032_LOW_MV          270   /* 2.70V → low (CR2032) */

/* === Emergency Dispatch === */
#define EMERGENCY_SMS_ENABLED      1
#define EMERGENCY_911_ENABLED      1
#define EMERGENCY_CONTACT_MAX      5

#endif /* GUIDESYNC_CONFIG_H */
# DriveSync Firmware — CMakeLists (top-level, for ESP-IDF hub build)
#
# This CMakeLists is for the ESP32-S3 Hub build via ESP-IDF.
# The nRF52840 nodes are built via PlatformIO (see platformio.ini).
# The RP2040 OBD dongle is built via Pico SDK (see obd-dongle/CMakeLists.txt).

cmake_minimum_required(VERSION 3.16)

# The ESP-IDF build system will include this directory.
# Only the hub/ subdirectory is built here.

# Hub (ESP32-S3)
idf_component_register(
    SRCS
        "hub/main.c"
        "hub/camera_driver.c"
        "hub/edge_inference.c"
        "hub/ble_central.c"
        "hub/wifi_mqtt.c"
        "common/protocol.c"
        "common/crc8.c"
        "common/crypto.c"
    INCLUDE_DIRS
        "hub"
        "common"
    REQUIRES
        "esp_camera"
        "nvs_flash"
        "esp_wifi"
        "esp_event"
        "esp_timer"
        "driver"
)
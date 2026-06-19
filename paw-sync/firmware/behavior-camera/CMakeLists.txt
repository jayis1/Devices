# PawSync Behavior Camera — CMakeLists.txt
# ESP32-S3 (ESP-IDF) firmware build configuration

cmake_minimum_required(VERSION 3.16)

# ESP-IDF build system (uses idf.py)
# This file is processed by the ESP-IDF CMake build system

idf_component_register(
    SRCS
        "camera_main.c"
        "behavior_model.c"
        "vocalization.c"
        "privacy.c"
        "../common/paw_protocol.c"
        "../common/mesh_model.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        esp_wifi
        esp_event
        esp_timer
        driver
        nvs_flash
)
# PawSync Smart Feeder — CMakeLists.txt
# ESP32-C6 (ESP-IDF) firmware build configuration

cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS
        "feeder_main.c"
        "load_cell.c"
        "rfid_pet.c"
        "schedule.c"
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
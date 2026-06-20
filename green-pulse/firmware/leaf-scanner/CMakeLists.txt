cmake_minimum_required(VERSION 3.13)
# GreenPulse Leaf Scanner — ESP32-S3 (ESP-IDF)
# Build: idf.py build flash monitor

set(CMAKE_C_STANDARD 11)

project(greenpulse_scanner C)

add_executable(scanner_main scanner_main.c)

target_include_directories(scanner_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# ESP-IDF components: freertos, esp_system, driver (gpio/spi/i2c),
# esp_timer, esp_camera, tflite-micro-esp
# idf_component_register(SRCS "scanner_main.c"
#     INCLUDE_DIRS "." "../common"
#     REQUIRES driver esp_camera tflite_micro)
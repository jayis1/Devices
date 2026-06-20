cmake_minimum_required(VERSION 3.13)
# GreenPulse Water Valve — ESP32-C6 (ESP-IDF)
# Build: idf.py build flash monitor

set(CMAKE_C_STANDARD 11)

project(greenpulse_valve C)

add_executable(valve_main valve_main.c)

target_include_directories(valve_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# ESP-IDF components: freertos, esp_system, driver (gpio/pcnt), esp_timer
# idf_component_register(SRCS "valve_main.c"
#     INCLUDE_DIRS "." "../common"
#     REQUIRES driver esp_timer)
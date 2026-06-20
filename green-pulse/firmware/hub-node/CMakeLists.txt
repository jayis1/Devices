cmake_minimum_required(VERSION 3.13)
# GreenPulse Hub Node — RP2040 (Pico SDK)
# Build: mkdir build && cd build && cmake .. && make -j4

set(CMAKE_C_STANDARD 11)

include(pico_sdk_import.cmake)

project(greenpulse_hub C CXX)

pico_sdk_init()

add_executable(hub_main hub_main.c)

target_include_directories(hub_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(hub_main PRIVATE
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_gpio
    pico_multicore
)

pico_enable_stdio_usb(hub_main 1)
pico_add_extra_outputs(hub_main)
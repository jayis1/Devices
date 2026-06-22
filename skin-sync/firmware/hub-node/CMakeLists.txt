cmake_minimum_required(VERSION 3.13)
project(skinsync_hub C)

set(CMAKE_C_STANDARD 11)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

add_executable(hub_main hub_main.c)
target_include_directories(hub_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(hub_main pico_stdlib hardware_spi hardware_i2c
    hardware_gpio hardware_uart pico_multicore)

pico_enable_stdio_usb(hub_main 1)
pico_add_extra_outputs(hub_main)
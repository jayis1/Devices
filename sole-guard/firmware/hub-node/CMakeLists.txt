# Hub Node — CMakeLists.txt (Pico SDK)
cmake_minimum_required(VERSION 3.12)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(sole_hub C CXX)

set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(sole_hub
    hub_main.c
    ulcerrisk_model.c
    heatmap_render.c
    ../common/sole_protocol.c
)
target_link_libraries(sole_hub
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_gpio
    hardware_uart
    pico_multicore
    tflite_micro
    ili9488
    max98357
)
target_include_directories(sole_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)
pico_add_extra_outputs(sole_hub)
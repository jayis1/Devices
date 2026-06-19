# PawSync Hub Node — CMakeLists.txt
# RP2040 (Pico SDK) firmware build configuration

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pawsync_hub C CXX)

pico_sdk_init()

add_executable(hub_node
    hub_main.c
    wellness_model.c
    vitals_render.c
    enrichment.c
    ../common/paw_protocol.c
    ../common/mesh_model.c
)

target_include_directories(hub_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(hub_node PRIVATE
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_gpio
    hardware_adc
)

pico_add_extra_outputs(hub_node)
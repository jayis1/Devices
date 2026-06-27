# Soil Probe CMakeLists.txt (Pico SDK)
# firmware/soil-probe/CMakeLists.txt

cmake_minimum_required(VERSION 3.13)

# This is a Pico SDK project
set(PICO_SDK_PATH ${PICO_SDK_PATH} CACHE PATH "Path to Pico SDK")

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(compostsync_soil_probe C CXX)

pico_sdk_init()

add_executable(soil_probe
    main.c
    ../common/csp_protocol.c
)

target_include_directories(soil_probe PRIVATE
    .
    ../common
)

target_link_libraries(soil_probe PRIVATE
    pico_stdlib
    pico_sleep
    hardware_gpio
    hardware_adc
    hardware_i2c
    hardware_spi
    hardware_uart
    hardware_watchdog
)

pico_enable_stdio_usb(soil_probe 1)
pico_enable_stdio_uart(soil_probe 0)

pico_add_extra_outputs(soil_probe)
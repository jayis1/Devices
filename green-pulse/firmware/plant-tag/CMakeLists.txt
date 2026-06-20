cmake_minimum_required(VERSION 3.13)
# GreenPulse Plant Tag — nRF52832 (nRF5 SDK)
# Build: requires nRF5 SDK + arm-none-eabi-gcc
# See https://www.nordicsemi.com/Products/Development-software/nrf5-sdk

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)

project(greenpulse_plant_tag C)

add_executable(plant_main plant_main.c)

target_include_directories(plant_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${NRF5_SDK_PATH}/components/libraries/scheduler
    ${NRF5_SDK_PATH}/modules/nrfx
)

# Link: nRF5 SDK softdevice (S112 for BLE peripheral),
# SX1262 driver, SAADC, TWI, RTC
target_link_libraries(plant_main PRIVATE
    # nrf5_sdk_softdevice_s112
    # nrfx_saadc
    # nrfx_twi
)
# Weather Station CMakeLists.txt (nRF SDK)
# firmware/weather-station/CMakeLists.txt

cmake_minimum_required(VERSION 3.13)

# This is an nRF5 SDK project (not Zephyr)
# Build with: make -C firmware/weather-station

set(NRF_SDK_PATH ${NRF_SDK_PATH} CACHE PATH "Path to nRF5 SDK")

project(compostsync_weather_station C)

set(SOURCES
    main.c
    power.c
    ../common/csp_protocol.c
    ../common/sx1262_driver.c
)

set(INCLUDE_DIRS
    .
    ../common
    ${NRF_SDK_PATH}/components/libraries/util
    ${NRF_SDK_PATH}/components/drivers_nrf/gpiote
    ${NRF_SDK_PATH}/components/drivers_nrf/twi
    ${NRF_SDK_PATH}/components/drivers_nrf/spi_master
    ${NRF_SDK_PATH}/components/drivers_nrf/saadc
    ${NRF_SDK_PATH}/components/drivers_nrf/clock
    ${NRF_SDK_PATH}/components/drivers_nrf/timer
    ${NRF_SDK_PATH}/components/libraries/util
    ${NRF_SDK_PATH}/modules/nrfx
    ${NRF_SDK_PATH}/integration/nrfx
    ${NRF_SDK_PATH}/components/boards
)

add_executable(weather_station ${SOURCES})
target_include_directories(weather_station PRIVATE ${INCLUDE_DIRS})
# CalmGrid Wrist Band — CMakeLists.txt
# nRF52840 (nRF5 SDK) firmware build configuration

cmake_minimum_required(VERSION 3.13)

project(calmgrid_wrist C)

set(NRF5_SDK_PATH "$ENV{NRF5_SDK_PATH}" CACHE PATH "nRF5 SDK path")

add_executable(wrist_band
    wrist_main.c
    ppg_hrv.c
    eda_gsr.c
    activity_classify.c
    stress_detect.c
    ../common/calm_protocol.c
    ../common/mesh_model.c
)

target_include_directories(wrist_band PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${NRF5_SDK_PATH}/components
    ${NRF5_SDK_PATH}/components/drivers_nrf
    ${NRF5_SDK_PATH}/components/libraries
    ${NRF5_SDK_PATH}/components/softdevice/s140/headers
)

target_link_libraries(wrist_band PRIVATE
    # nRF5 SDK libraries (linked via Makefile in production)
)
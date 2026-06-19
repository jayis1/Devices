# PawSync Collar Tag — CMakeLists.txt
# nRF52840 (nRF5 SDK) firmware build configuration

cmake_minimum_required(VERSION 3.13)

project(pawsync_collar C)

# nRF5 SDK path (set via -DNRF5_SDK_PATH=...)
set(NRF5_SDK_PATH "$ENV{NRF5_SDK_PATH}" CACHE PATH "nRF5 SDK path")

add_executable(collar_tag
    collar_main.c
    ppg_hr.c
    activity_cnn.c
    gait_analysis.c
    scratch_detect.c
    ../common/paw_protocol.c
    ../common/mesh_model.c
)

target_include_directories(collar_tag PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${NRF5_SDK_PATH}/components
    ${NRF5_SDK_PATH}/components/drivers_nrf
    ${NRF5_SDK_PATH}/components/libraries
    ${NRF5_SDK_PATH}/components/softdevice/s140/headers
)

target_link_libraries(collar_tag PRIVATE
    # nRF5 SDK libraries (linked via Makefile in production)
)
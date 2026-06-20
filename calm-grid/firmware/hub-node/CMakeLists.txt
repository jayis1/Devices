# CalmGrid Hub Node — CMakeLists.txt
# RP2040 (Pico SDK) firmware build configuration

cmake_minimum_required(VERSION 3.13)

project(calmgrid_hub C)

set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}" CACHE PATH "Pico SDK path")

add_executable(hub_node
    hub_main.c
    stress_model.c
    intervention.c
    dashboard_render.c
    ../common/calm_protocol.c
    ../common/mesh_model.c
)

target_include_directories(hub_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${PICO_SDK_PATH}/src
)

target_link_libraries(hub_node PRIVATE
    # Pico SDK libraries (linked via pico_sdk_import in production)
)
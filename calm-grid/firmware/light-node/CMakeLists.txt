# CalmGrid Light Node — CMakeLists.txt
# ESP32-C6 (ESP-IDF) firmware build configuration

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(calmgrid_light)

set(SRCS
    "light_main.c"
    "circadian.c"
    "../common/calm_protocol.c"
    "../common/mesh_model.c"
)

idf_component_register(SRCS ${SRCS}
    INCLUDE_DIRS "." "../common"
)
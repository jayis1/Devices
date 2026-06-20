# CalmGrid Room Sentinel — CMakeLists.txt
# ESP32-S3 (ESP-IDF) firmware build configuration

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(calmgrid_sentinel)

# Components
set(SRCS
    "sentinel_main.c"
    "prosody.c"
    "ambient.c"
    "privacy.c"
    "../common/calm_protocol.c"
)

idf_component_register(SRCS ${SRCS}
    INCLUDE_DIRS "." "../common"
)
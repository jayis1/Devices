# JointSync Firmware — Top-Level CMakeLists
#
# Builds all firmware targets using PlatformIO-compatible structure.
# Each node has its own CMakeLists.txt.

cmake_minimum_required(VERSION 3.16)

project(jointsync_firmware VERSION 1.0.0)

# Common protocol library
add_library(jointsync_common STATIC
    common/protocol.c
    common/crc8.c
    common/crypto.c
)
target_include_directories(jointsync_common PUBLIC common)

# Node-specific builds are handled by PlatformIO
# See platformio.ini for build configurations
message(STATUS "JointSync firmware — use 'pio run' to build individual targets")
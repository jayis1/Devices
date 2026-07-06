# SightSync Firmware CMakeLists (for ESP-IDF builds)
# License: MIT
#
# This file allows building ESP-IDF firmware (hub, desk-sentinel) with CMake
# outside of PlatformIO. The nRF52840 (eye-tag) and RP2040 (lamp-node) use
# Arduino-Pico/nRF52 frameworks via PlatformIO.

cmake_minimum_required(VERSION 3.16)

# When built via PlatformIO, this file is not used.
# For standalone ESP-IDF builds:
if(DEFINED ENV{IDF_PATH})
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(sightsync-firmware)
endif()
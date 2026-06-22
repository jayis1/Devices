cmake_minimum_required(VERSION 3.13)
project(skinsync_uv_patch C)

set(CMAKE_C_STANDARD 11)
# nRF5 SDK build (uses arm-none-eabi-gcc + nRF5 SDK makefiles)
# Build: make SDK_ROOT=~/nRF5_SDK TARGET=uv_patch_main
target_include_directories(uv_patch_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
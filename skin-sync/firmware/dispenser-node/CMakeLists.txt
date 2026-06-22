cmake_minimum_required(VERSION 3.16)
project(skinsync_dispenser C)

set(CMAKE_C_STANDARD 11)
# ESP-IDF build: idf.py build
# Requires ESP-IDF v5.x
target_include_directories(dispenser_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
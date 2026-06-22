cmake_minimum_required(VERSION 3.16)
project(skinsync_scanner C)

set(CMAKE_C_STANDARD 11)
# ESP-IDF build: idf.py build
# Requires ESP-IDF v5.x with esp_camera + tflite-micro-esp-examples components
target_include_directories(scanner_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
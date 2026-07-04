# GlucoSync Firmware — CMakeLists.txt
# For ESP32-S3 nodes (Hub, Meal Scanner) using ESP-IDF.
# nRF52840 nodes (Activity Band, Pen Tag) use PlatformIO.

cmake_minimum_required(VERSION 3.16)

# This file is for reference. Each ESP32-S3 node has its own
# CMakeLists.txt in its subdirectory for ESP-IDF.
#
# Hub:       firmware/hub/CMakeLists.txt → idf.py build
# Scanner:   firmware/meal-scanner/CMakeLists.txt → idf.py build
#
# Common code is included via:
# idf_component_register(SRCS "main.c" "../common/protocol.c" ...
#                        INCLUDE_DIRS "." "../common")

project(glucosync_firmware C)
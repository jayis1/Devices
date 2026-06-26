cmake_minimum_required(VERSION 3.20)
project(trail-sync-firmware C)

# Shared protocol library
add_library(trail_protocol STATIC
    common/trail_protocol.c
)
target_include_directories(trail_protocol PUBLIC common)

# Wrist Unit (nRF52832)
add_executable(wrist-unit wrist-unit/wrist_main.c)
target_link_libraries(wrist-unit trail_protocol)

# Shoe Pod (nRF52833)
add_executable(shoe-pod shoe-pod/shoe_pod_main.c)
target_link_libraries(shoe-pod trail_protocol)

# Trail Beacon (nRF52833)
add_executable(trail-beacon trail-beacon/trail_beacon_main.c)
target_link_libraries(trail-beacon trail_protocol)

# Hub (ESP32-S3, built with ESP-IDF separately)
# add_executable(hub-node hub-node/hub_main.c)
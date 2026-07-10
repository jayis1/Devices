# AllergySync Wearable Tag — Zephyr CMakeLists
# Target: nRF52840

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(allergysync_wearable_tag)

target_sources(app PRIVATE
    src/main.c
    ../common/allergysync_proto.c
    ../common/as_lr1121.c
    ../common/as_tdma.c
)

target_include_directories(app PRIVATE
    .
    ../common
)
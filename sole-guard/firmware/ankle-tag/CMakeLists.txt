# Ankle Tag — CMakeLists.txt (Zephyr / nRF52840)
cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS ${ZEPHYR_BASE})
project(sole_ankle)
target_sources(app PRIVATE
    ankle_main.c
    bioimpedance.c
    ../common/sole_protocol.c
    ../common/mesh_model.c
)
target_compile_definitions(app PRIVATE NODE_ID=SOLE_NODE_ID_ANKLE)
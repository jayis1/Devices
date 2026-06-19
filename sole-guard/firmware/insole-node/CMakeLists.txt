# Insole Node — CMakeLists.txt (Zephyr)
#
# Builds for nRF52840. Override NODE_ID for left/right:
#   west build -b nrf52840dk_nrf52840 -- -DNODE_ID=SOLE_NODE_ID_INSOLE_L

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS ${ZEPHYR_BASE})

project(sole_insole)

target_sources(app PRIVATE
    insole_main.c
    pressure_matrix.c
    thermistor_array.c
    gait_phase.c
    ../common/sole_protocol.c
    ../common/mesh_model.c
)

target_compile_definitions(app PRIVATE
    NODE_ID=${NODE_ID}
)
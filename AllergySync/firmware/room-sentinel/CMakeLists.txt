# AllergySync Room Sentinel — ESP-IDF CMakeLists

idf_component_register(
    SRCS
        "main.c"
        "../common/allergysync_proto.c"
        "../common/as_lr1121.c"
        "../common/as_tdma.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        driver
        esp_timer
)
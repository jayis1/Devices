# AllergySync Hub — ESP-IDF CMakeLists
# Target: ESP32-S3-WROOM-1-N16R8

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
        esp_wifi
        esp_event
        nvs_flash
        spiffs
        mqtt
        mbedtls
        driver
)
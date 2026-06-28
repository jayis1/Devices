# PestSync Hub — CMakeLists.txt
# firmware/hub/CMakeLists.txt

idf_component_register(
    SRCS
        "main.c"
        "lora_mesh.c"
        "ble_service.c"
        "wifi_mqtt.c"
        "edge_ml.c"
        "display.c"
        "sd_logger.c"
        "../common/psp_protocol.c"
        "../common/sx1262_driver.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        freertos
        esp_wifi
        esp_event
        esp_timer
        nvs_flash
        driver
        esp_adc_cal
        nimble
        mqtt
)
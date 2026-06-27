# Hub CMakeLists.txt
# firmware/hub/CMakeLists.txt

# ESP-IDF project structure
idf_component_register(
    SRCS
        "main.c"
        "lora_mesh.c"
        "ble_service.c"
        "wifi_mqtt.c"
        "edge_ml.c"
        "display.c"
        "sd_logger.c"
        "../common/csp_protocol.c"
        "../common/sx1262_driver.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        freertos
        nvs_flash
        esp_wifi
        esp_event
        esp_bt
        esp_gatts_api
        mqtt
        driver
    EMBED_FILES
        # "model/phase_classifier.tflite"  # quantized CNN model
)
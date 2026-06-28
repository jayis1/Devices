# PestSync Pest Sentinel — CMakeLists.txt
# firmware/pest-sentinel/CMakeLists.txt

idf_component_register(
    SRCS
        "main.c"
        "camera.c"
        "pest_cnn.c"
        "thermal.c"
        "pir.c"
        "lora_node.c"
        "power.c"
        "../common/psp_protocol.c"
        "../common/sx1262_driver.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        freertos
        esp_timer
        nvs_flash
        driver
        esp_adc_cal
        esp_camera
)
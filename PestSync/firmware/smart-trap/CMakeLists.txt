# PestSync Smart Trap — CMakeLists.txt
# firmware/smart-trap/CMakeLists.txt

idf_component_register(
    SRCS
        "main.c"
        "trap_sensors.c"
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
)
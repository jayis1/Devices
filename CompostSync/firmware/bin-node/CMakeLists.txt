# Bin Node CMakeLists.txt
# firmware/bin-node/CMakeLists.txt

idf_component_register(
    SRCS
        "main.c"
        "sensors.c"
        "servo.c"
        "lora_node.c"
        "ble_bridge.c"
        "power.c"
        "../common/csp_protocol.c"
        "../common/sx1262_driver.c"
    INCLUDE_DIRS
        "."
        "../common"
    REQUIRES
        freertos
        nvs_flash
        esp_wifi
        esp_bt
        esp_gatts_api
        driver
)
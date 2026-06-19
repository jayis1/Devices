# Foot Scanner — CMakeLists.txt (ESP-IDF)
# Build with: idf.py build
idf_component_register(
    SRCS "scanner_main.c" "wound_model.c" "weight_scale.c"
    INCLUDE_DIRS "." "../common"
    REQUIRES esp_camera driver esp_wifi esp_http_client nvs_flash
)
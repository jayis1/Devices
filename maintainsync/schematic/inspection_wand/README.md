# Inspection Wand schematic notes

ESP32-S3 I²C GPIO8/9 connects MLX90640, VL53L5CX, OLED, and fuel gauge. PN532 UART uses GPIO17/18. OV5640 DVP follows Espressif camera pin constraints. BQ24074 charges the LiPo while supplying system load; add a physical lens shutter and keep thermal and RGB optical axes registered in enclosure.

# Gateway net specification

ESP32-S3-WROOM-1-N8 module: 5V USB to carrier regulator input; module 3V3 rail; common GND. GPIO8 SDA and GPIO9 SCL connect OLED module at 0x3C with pull-ups to 3V3. USB-UART/service interface to local Linux host is a carrier implementation detail. Add decoupling and programming/reset circuitry per Espressif reference design. This is not a schematic; confirm GPIO availability on selected module and display carrier.

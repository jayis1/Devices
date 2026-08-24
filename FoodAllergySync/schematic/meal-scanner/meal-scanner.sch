EESchema Schematic File Version 4
LIBS:foodallergysync-meal-scanner
$Descr A3 16535 11693
Title "FoodAllergySync Meal Scanner"
Date "2026-08-24"
Rev "A"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
ESP32-S3-WROOM-1 central SoC
Text Notes 1000 1500 0 50 ~ 0
OV5640 camera over DVP, GM65 barcode over UART, AS7341 + VL53L1X over I2C
Text Notes 1000 2000 0 50 ~ 0
HX711 load cell front end, ST7789 TFT over SPI, WS2812 LED ring, I2S speaker amp
Text Notes 1000 2500 0 50 ~ 0
12V input -> 5V buck -> 3V3 LDO/buck, ESD protection on user IO
$EndSCHEMATC

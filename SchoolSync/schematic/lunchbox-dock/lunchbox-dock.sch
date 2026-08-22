EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "SchoolSync Lunchbox Dock"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-S3 + HX711 + TMP117 + SHT41 dock"
Comment2 "Weight + temperature + pickup verification"
Comment3 "Power rails: 5V, 3V3"
Comment4 "Buses: GPIO, I2C, UART"
$EndDescr
$Comp
L Device:U U1
U 1 1 65000001
P 4200 2600
F 0 "U1" H 4200 3400 50 0000 C CNN
F 1 "ESP32-S3" H 4200 3300 50 0000 C CNN
F 2 "Module:ESP32-S3" H 4200 2600 50 0001 C CNN
F 3 "" H 4200 2600 50 0001 C CNN
1 4200 2600
1 0 0 -1
$EndComp
Text Notes 1200 7000 0 50 ~ 0
Interfaces:
GPIO -> HX711
I2C -> TMP117 + SHT41 + VL53L0X
GPIO -> reed switch and LEDs
$EndSCHEMATC

EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "RoutineSync Doorway Dock"
Date "2026-08-28"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-S3 + DWM3000 + HX711 + PN532"
Comment2 "Departure checklist tray and verification dock"
Comment3 "Power rails: 12V, 5V, 3V3"
Comment4 "Buses: SPI, I2C, GPIO, capacitive touch"
$EndDescr
$Comp
L Device:U U_ESP32S3
U 1 1 66000011
P 4100 2500
F 0 "U1" H 4100 3300 50 0000 C CNN
F 1 "ESP32-S3-WROOM-1" H 4100 3200 50 0000 C CNN
F 2 "RF_Module:ESP32-S3-WROOM-1" H 4100 2500 50 0001 C CNN
F 3 "" H 4100 2500 50 0001 C CNN
1 4100 2500
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
SPI -> DWM3000 + e-paper
I2C -> PN532 + VL53L1X
GPIO -> reed switch + buzzer + LED bar
HX711 -> 4-load-cell tray mass sensing
$EndSCHEMATC

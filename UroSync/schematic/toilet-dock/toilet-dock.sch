EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "UroSync Toilet Dock Analyzer"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-S3 urine strip reader + uroflow sidecar"
Comment2 "Power rails: 12V, 5V, 3V3_A"
Comment3 "Interfaces: I2C, I2S, GPIO, stepper driver"
$EndDescr
$Comp
L Device:U U_ESP32S3
U 1 1 70000011
P 3600 2600
F 0 "U1" H 3600 3400 50 0000 C CNN
F 1 "ESP32-S3-WROOM-1" H 3600 3300 50 0000 C CNN
1 3600 2600
1 0 0 -1
$EndComp
Text Notes 1000 6800 0 50 ~ 0
I2C -> AS7341 + AD5933 + ADS1115 + VL53L0X
I2S -> ICS-43434 microphone
GPIO -> HX711 + ULN2003 + capacitive pads
12V input -> buck5V -> LDO3V3_A
$EndSCHEMATC

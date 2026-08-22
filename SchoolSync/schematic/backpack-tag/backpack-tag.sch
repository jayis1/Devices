EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "SchoolSync Backpack Tag"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "nRF5340 + DW3110 + SX1262 wearable/bag tag"
Comment2 "Motion + hall + NFC + LiPo"
Comment3 "Power rails: VBAT, 3V3"
Comment4 "Buses: SPI, I2C, GPIO, NFC"
$EndDescr
$Comp
L Device:U U1
U 1 1 65000001
P 4200 2600
F 0 "U1" H 4200 3400 50 0000 C CNN
F 1 "nRF5340" H 4200 3300 50 0000 C CNN
F 2 "Module:nRF5340" H 4200 2600 50 0001 C CNN
F 3 "" H 4200 2600 50 0001 C CNN
1 4200 2600
1 0 0 -1
$EndComp
Text Notes 1200 7000 0 50 ~ 0
Interfaces:
SPI -> DW3110 and SX1262
I2C -> SHTC3
GPIO -> hall sensors, button, LED
ADC -> battery monitor
$EndSCHEMATC

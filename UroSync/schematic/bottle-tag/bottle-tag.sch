EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "UroSync Hydration Bottle Tag"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "nRF52840 + SX1262 smart bottle coaster"
Comment2 "Power rails: VBAT, 3V3"
Comment3 "Interfaces: SPI, I2C, GPIO, charger"
$EndDescr
$Comp
L Device:U U_NRF52
U 1 1 70000031
P 3800 2700
F 0 "U1" H 3800 3500 50 0000 C CNN
F 1 "nRF52840" H 3800 3400 50 0000 C CNN
1 3800 2700
1 0 0 -1
$EndComp
Text Notes 1000 6800 0 50 ~ 0
SPI -> SX1262
GPIO -> HX711 + coin motor driver
I2C -> LIS2DW12
USB-C -> MCP73831 -> 1200mAh LiPo
$EndSCHEMATC

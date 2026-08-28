EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "RoutineSync Object Tag"
Date "2026-08-28"
Rev "1.0"
Comp "Devices"
Comment1 "nRF52840 + DWM3000 + LIS2DW12"
Comment2 "Findable tag for keys, wallet, backpack, and essentials"
Comment3 "Power rails: 4.2V, 3V3"
Comment4 "Buses: SPI, I2C, GPIO"
$EndDescr
$Comp
L Device:U U_NRF52840
U 1 1 66000031
P 4100 2500
F 0 "U1" H 4100 3300 50 0000 C CNN
F 1 "nRF52840-QIAA" H 4100 3200 50 0000 C CNN
F 2 "Package_QFN:QFN-73-1EP_7x7mm" H 4100 2500 50 0001 C CNN
F 3 "" H 4100 2500 50 0001 C CNN
1 4100 2500
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
SPI -> DWM3000
I2C -> LIS2DW12
GPIO -> buzzer + RGB LED + side button
LiPo charger -> MCP73831
$EndSCHEMATC

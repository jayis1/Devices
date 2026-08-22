EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WasteSort Pickup Beacon"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "nRF52840 + SX1262 + LIS2DW12 + solar charging"
Comment2 "Outdoor curb placement and pickup verification beacon"
Comment3 "Power rails: SOLAR, BAT, 3V3"
Comment4 "Buses: BLE, SPI, I2C, GPIO"
$EndDescr

$Comp
L power:+3V3 #PWR01
U 1 1 65000001
P 1800 1000
F 0 "#PWR01" H 1800 850 50 0001 C CNN
F 1 "+3V3" H 1815 1173 50 0000 C CNN
F 2 "" H 1800 1000 50 0001 C CNN
F 3 "" H 1800 1000 50 0001 C CNN
1 1800 1000
1 0 0 -1
$EndComp

$Comp
L Device:U U_NRF52840
U 1 1 65000002
P 4200 2600
F 0 "U1" H 4200 3400 50 0000 C CNN
F 1 "nRF52840" H 4200 3300 50 0000 C CNN
F 2 "Module:nRF52840" H 4200 2600 50 0001 C CNN
F 3 "" H 4200 2600 50 0001 C CNN
1 4200 2600
1 0 0 -1
$EndComp

$Comp
L Device:U U_SX1262
U 1 1 65000003
P 7600 2600
F 0 "U2" H 7600 3400 50 0000 C CNN
F 1 "SX1262" H 7600 3300 50 0000 C CNN
F 2 "Module:SX1262" H 7600 2600 50 0001 C CNN
F 3 "" H 7600 2600 50 0001 C CNN
1 7600 2600
1 0 0 -1
$EndComp

Text Notes 1200 7000 0 50 ~ 0
Interfaces:
SPI -> SX1262 + e-paper
I2C -> LIS2DW12 + BMA400
GPIO -> hall dock sensor
SOLAR -> CN3791 -> 32700 LiFePO4
$EndSCHEMATC

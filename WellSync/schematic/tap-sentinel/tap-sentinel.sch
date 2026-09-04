EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WellSync Tap Sentinel"
Date "2026-09-04"
Rev "1.0"
Comp "Devices"
Comment1 "nRF52840 + TMP117 + OPT3001 + PN532"
Comment2 "Point-of-use treatment verification"
Comment3 "Rails: Li-ion 3V7, 3V3"
Comment4 "Buses: I2C, SPI, BLE"
$EndDescr
$Comp
L Device:U U1
U 1 1 70000031
P 4700 2600
F 0 "U1" H 4700 3450 50 0000 C CNN
F 1 "nRF52840" H 4700 3350 50 0000 C CNN
1 4700 2600
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
Inputs:
TMP117 -> pipe temperature
OPT3001 -> UV indicator or service LED
Hall/reed pulse -> faucet use
PN532 -> filter cartridge NFC registration
Cap sense -> cabinet door event
$EndSCHEMATC

EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "SchoolSync Family Hub"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "Raspberry Pi CM4 + RP2040 + SX1262 gateway"
Comment2 "Display + speaker + UPS supervisor"
Comment3 "Power rails: 12V, 5V, 3V3"
Comment4 "Buses: SPI, I2C, UART, DSI, Ethernet"
$EndDescr
$Comp
L Device:U U1
U 1 1 65000001
P 4200 2600
F 0 "U1" H 4200 3400 50 0000 C CNN
F 1 "RP2040" H 4200 3300 50 0000 C CNN
F 2 "Module:RP2040" H 4200 2600 50 0001 C CNN
F 3 "" H 4200 2600 50 0001 C CNN
1 4200 2600
1 0 0 -1
$EndComp
Text Notes 1200 7000 0 50 ~ 0
Interfaces:
RP2040 SPI0 -> SX1262
RP2040 UART0 -> CM4
CM4 DSI -> family display
CM4 I2S -> MAX98357A speaker
$EndSCHEMATC

EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "UroSync Mirror Hub"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "CM4 + RP2040 + SX1262 + mirror display"
Comment2 "Power rails: 12V, 5V, 3V3"
Comment3 "Interfaces: SPI, I2C, UART, DSI, I2S, Ethernet"
$EndDescr
$Comp
L Device:U U_RP2040
U 1 1 70000001
P 3800 2600
F 0 "U1" H 3800 3400 50 0000 C CNN
F 1 "RP2040" H 3800 3300 50 0000 C CNN
1 3800 2600
1 0 0 -1
$EndComp
$Comp
L Device:U U_CM4
U 1 1 70000002
P 7600 2500
F 0 "U2" H 7600 3300 50 0000 C CNN
F 1 "Raspberry_Pi_CM4" H 7600 3200 50 0000 C CNN
1 7600 2500
1 0 0 -1
$EndComp
Text Notes 1000 6800 0 50 ~ 0
SPI0 -> SX1262
I2C0 -> DS3231 + INA219
UART0 -> CM4 watchdog link
DSI -> 8in mirror LCD
I2S -> MAX98357A speaker amp
$EndSCHEMATC

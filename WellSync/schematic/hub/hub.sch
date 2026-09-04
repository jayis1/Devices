EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WellSync Hub Gateway"
Date "2026-09-04"
Rev "1.0"
Comp "Devices"
Comment1 "CM4 + RP2040 + SX1262 + LTE"
Comment2 "Edge orchestration + OTA + MQTT bridge"
Comment3 "Rails: 12V, 5V, 3V3, LiFePO4 backup"
Comment4 "Buses: SPI, I2C, UART, USB2, Ethernet"
$EndDescr
$Comp
L Device:U U1
U 1 1 70000001
P 4300 2500
F 0 "U1" H 4300 3300 50 0000 C CNN
F 1 "RP2040" H 4300 3200 50 0000 C CNN
1 4300 2500
1 0 0 -1
$EndComp
$Comp
L Device:U U2
U 1 1 70000002
P 7600 2500
F 0 "U2" H 7600 3300 50 0000 C CNN
F 1 "Raspberry_Pi_CM4" H 7600 3200 50 0000 C CNN
1 7600 2500
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
Interfaces:
SPI0 -> SX1262
UART0 -> CM4 supervisor link
USB2 -> EG25-G LTE modem
I2C1 -> INA219 + DS3231 + ATECC608
GPIO -> stack light + mute + service relay
$EndSCHEMATC

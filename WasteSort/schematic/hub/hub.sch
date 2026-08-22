EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WasteSort Hub Gateway"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "Raspberry Pi CM4 + RP2040 + SX1262 coordinator"
Comment2 "Touch dashboard + MQTT bridge + UPS supervisor"
Comment3 "Power rails: 12V, 5V, 3V3"
Comment4 "Buses: SPI, I2C, UART, DSI, Ethernet"
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
L Device:U U_RP2040
U 1 1 65000002
P 4200 2600
F 0 "U1" H 4200 3400 50 0000 C CNN
F 1 "RP2040" H 4200 3300 50 0000 C CNN
F 2 "Module:RP2040" H 4200 2600 50 0001 C CNN
F 3 "" H 4200 2600 50 0001 C CNN
1 4200 2600
1 0 0 -1
$EndComp

$Comp
L Device:U U_CM4
U 1 1 65000003
P 7600 2600
F 0 "U2" H 7600 3400 50 0000 C CNN
F 1 "Raspberry_Pi_CM4" H 7600 3300 50 0000 C CNN
F 2 "Module:Raspberry_Pi_CM4" H 7600 2600 50 0001 C CNN
F 3 "" H 7600 2600 50 0001 C CNN
1 7600 2600
1 0 0 -1
$EndComp

Text Notes 1200 7000 0 50 ~ 0
Interfaces:
RP2040 SPI0 -> SX1262
RP2040 UART0 -> CM4 console
CM4 DSI -> 5in touch display
CM4 ETH -> RJ45 PHY
I2C -> RTC + INA219
$EndSCHEMATC

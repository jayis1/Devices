EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "RoutineSync Hub Gateway"
Date "2026-08-28"
Rev "1.0"
Comp "Devices"
Comment1 "CM4 + RP2040 + nRF52840 + DWM3001C"
Comment2 "Edge automation and BLE/UWB coordination"
Comment3 "Power rails: 12V, 5V, 3V3"
Comment4 "Buses: SPI, UART, I2C, DSI, Ethernet"
$EndDescr
$Comp
L Device:U U_RP2040
U 1 1 66000001
P 3600 2400
F 0 "U1" H 3600 3200 50 0000 C CNN
F 1 "RP2040" H 3600 3100 50 0000 C CNN
F 2 "Module:RP2040" H 3600 2400 50 0001 C CNN
F 3 "" H 3600 2400 50 0001 C CNN
1 3600 2400
1 0 0 -1
$EndComp
$Comp
L Device:U U_CM4
U 1 1 66000002
P 7200 2400
F 0 "U2" H 7200 3200 50 0000 C CNN
F 1 "Raspberry_Pi_CM4" H 7200 3100 50 0000 C CNN
F 2 "Module:Raspberry_Pi_CM4" H 7200 2400 50 0001 C CNN
F 3 "" H 7200 2400 50 0001 C CNN
1 7200 2400
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
RP2040 SPI0 -> DWM3001C
RP2040 UART0 -> nRF52840 coprocessor
CM4 DSI -> 5in display
CM4 ETH -> RJ45
I2C -> RTC + INA219 + LED driver
$EndSCHEMATC

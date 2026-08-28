EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "RoutineSync Focus Beacon"
Date "2026-08-28"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-C6 + LD2410B + SGP40 + TSL2591"
Comment2 "Room focus-state sensing and cue rendering"
Comment3 "Power rails: 5V, 3V3"
Comment4 "Buses: UART, I2C, I2S, GPIO"
$EndDescr
$Comp
L Device:U U_ESP32C6
U 1 1 66000021
P 4200 2500
F 0 "U1" H 4200 3300 50 0000 C CNN
F 1 "ESP32-C6-WROOM-1" H 4200 3200 50 0000 C CNN
F 2 "RF_Module:ESP32-C6-WROOM-1" H 4200 2500 50 0001 C CNN
F 3 "" H 4200 2500 50 0001 C CNN
1 4200 2500
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
UART -> LD2410B mmWave
I2C -> SGP40 + TSL2591 + DRV2605L
I2S -> ICS-43434 microphone
GPIO -> LED ring + relays + focus buttons
$EndSCHEMATC

EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "CleanSync Dirt Sentinel"
Date "2026-08-27"
Rev "1.0"
Comp "Devices"
Comment1 "STM32WL55 + PM1006K + SHT41 + LD2410B"
Comment2 "Room dirt and wet-floor risk sensing"
Comment3 "Power rails: AA battery/USB-C, 3V3"
Comment4 "Buses: UART, I2C, GPIO, Sub-GHz RF"
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

Text Notes 1200 7000 0 50 ~ 0
UART -> PM1006K
I2C -> SHT41 + VEML6035 + SGP40
GPIO -> LD2410B presence + LED + button
SPI/RF -> integrated STM32WL radio path
$EndSCHEMATC

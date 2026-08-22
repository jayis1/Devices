EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "UroSync Bath Sentinel"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "STM32WL bathroom environment and leak node"
Comment2 "Power rails: 12V, 5V, 3V3"
Comment3 "Interfaces: I2C, GPIO, relay, MOSFET"
$EndDescr
$Comp
L Device:U U_STM32WL
U 1 1 70000041
P 3900 2600
F 0 "U1" H 3900 3400 50 0000 C CNN
F 1 "STM32WL55JC" H 3900 3300 50 0000 C CNN
1 3900 2600
1 0 0 -1
$EndComp
Text Notes 1000 6800 0 50 ~ 0
I2C -> SHT31 + SGP41 + VEML7700
GPIO -> leak strip comparator + fan relay + night-light MOSFET
12V input -> 5V buck -> 3V3 LDO
$EndSCHEMATC

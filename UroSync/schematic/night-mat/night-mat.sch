EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "UroSync Night Safety Mat"
Date "2026-08-22"
Rev "1.0"
Comp "Devices"
Comment1 "STM32L4 balance mat + path light"
Comment2 "Power rails: 5V, 3V3"
Comment3 "Interfaces: I2C, SPI, GPIO"
$EndDescr
$Comp
L Device:U U_STM32L4
U 1 1 70000021
P 3800 2700
F 0 "U1" H 3800 3500 50 0000 C CNN
F 1 "STM32L4R5" H 3800 3400 50 0000 C CNN
1 3800 2700
1 0 0 -1
$EndComp
Text Notes 1000 6800 0 50 ~ 0
ADS1232 <- 4x load cells
I2C -> TMP117 + VL53L5CX + BMA400
GPIO -> amber LED strip MOSFET + piezo
Optional LiFePO4 backup path
$EndSCHEMATC

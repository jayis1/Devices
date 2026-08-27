EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "CleanSync Surface Wand"
Date "2026-08-27"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-S3 + RP2040 + OV5640 + AS7341"
Comment2 "Handheld residue verification and scan logging"
Comment3 "Power rails: Li-ion, 5V, 3V3"
Comment4 "Buses: DVP, I2C, SPI, USB-C"
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
DVP -> OV5640 camera
I2C -> AS7341 + OPT4048 + VL53L1X + MAX17048
GPIO/PWM -> UV-A LED pulses + haptic
USB-C -> BQ25895 charging path
$EndSCHEMATC

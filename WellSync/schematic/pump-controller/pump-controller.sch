EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WellSync Pump & Pressure Controller"
Date "2026-09-04"
Rev "1.0"
Comp "Devices"
Comment1 "ESP32-S3 + ADE7953 + ADS1115 + SX1262"
Comment2 "Pump health, pressure control, contactor interlock"
Comment3 "Rails: 24V, 5V, 3V3"
Comment4 "Buses: SPI, I2C, UART"
$EndDescr
$Comp
L Device:U U1
U 1 1 70000021
P 4700 2600
F 0 "U1" H 4700 3450 50 0000 C CNN
F 1 "ESP32-S3-WROOM-1" H 4700 3350 50 0000 C CNN
1 4700 2600
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
Inputs:
ADE7953 -> pump current signature
ADS1115 -> dual pressure transducers
DAC/PWM -> VFD 0-10V reference
GPIO -> opto-isolated relay + buzzer + estop sense
$EndSCHEMATC

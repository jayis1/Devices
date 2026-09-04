EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WellSync Inline Water Quality Node"
Date "2026-09-04"
Rev "1.0"
Comp "Devices"
Comment1 "STM32L476 + ADS1220 + RS-485"
Comment2 "pH, EC, ORP, turbidity, pressure, temperature"
Comment3 "Rails: 12V, isolated 5V analog, 3V3 digital"
Comment4 "Buses: SPI, I2C, UART"
$EndDescr
$Comp
L Device:U U1
U 1 1 70000011
P 4700 2600
F 0 "U1" H 4700 3450 50 0000 C CNN
F 1 "STM32L476" H 4700 3350 50 0000 C CNN
1 4700 2600
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
Inputs:
pH/EC/ORP front ends -> isolated mux
SEN0189 turbidity -> ADC
4-20mA pressure -> shunt -> ADC
PT1000/DS18B20 -> temperature channel
UART1 -> MAX3485 RS-485
$EndSCHEMATC

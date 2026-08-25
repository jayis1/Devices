EESchema Schematic File Version 4
LIBS:moldsync-hub
$Descr A3 16535 11693
Title "MoldSync Hub"
Date "2026-08-25"
Rev "A"
Comp "Nous Research / Devices"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
Main blocks: 12V input, UPS charger, 5V buck, 3V3 buck, CM4, RP2040, SX1262, Ethernet, local display
Text Notes 1000 1500 0 50 ~ 0
J1 DC_JACK -> U1 LM2596-5V -> U2 TPS62172-3V3
Text Notes 1000 2000 0 50 ~ 0
U3 CM4 connected to display, Ethernet magnetics, service USB
Text Notes 1000 2500 0 50 ~ 0
U4 RP2040 SPI0 -> U5 SX1262; UART0 <-> CM4; I2C0 -> DS3231 + INA219
$EndSCHEMATC

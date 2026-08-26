EESchema Schematic File Version 4
LIBS:mobilitysync-hub
$Descr A3 16535 11693
Title "MobilitySync Hub"
Date "2026-08-26"
Rev "A"
Comp "Nous Research / Devices"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
Main blocks: 12V input, UPS charger, 5V buck, 3V3 buck, CM4, RP2040, SX1262, DW3110, Ethernet, touchscreen
Text Notes 1000 1500 0 50 ~ 0
J1 DC_JACK -> U1 MP1584 -> 5V rail -> U2 TPS62172 -> 3V3 rail
Text Notes 1000 2000 0 50 ~ 0
U3 CM4 connected to Ethernet magnetics, DSI touchscreen, service USB-C
Text Notes 1000 2500 0 50 ~ 0
U4 RP2040 SPI0->U5 SX1262, SPI1->U6 DW3110, UART0<->CM4, I2C0->RTC+INA219
$EndSCHEMATC

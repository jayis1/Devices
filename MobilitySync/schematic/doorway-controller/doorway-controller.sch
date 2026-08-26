EESchema Schematic File Version 4
LIBS:mobilitysync-doorway
$Descr A3 16535 11693
Title "MobilitySync Doorway Controller"
Date "2026-08-26"
Rev "A"
Comp "Nous Research / Devices"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
Main blocks: 24VDC input, 5V/3V3 rails, STM32G474, SX1262, DW3110, DRV8876 door motor driver, AS5600 angle sensor, VL53L1X, electric strike, RGB threshold LEDs
Text Notes 1000 1500 0 50 ~ 0
U1 STM32G474 SPI->SX1262 and DW3110; I2C->AS5600/VL53L1X; GPIO->strike, reed switch, obstruction beam
$EndSCHEMATC

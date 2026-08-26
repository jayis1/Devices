EESchema Schematic File Version 4
LIBS:mobilitysync-walker
$Descr A3 16535 11693
Title "MobilitySync Smart Walker"
Date "2026-08-26"
Rev "A"
Comp "Nous Research / Devices"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
Main blocks: 24V battery input, 5V buck, 3V3 LDO, nRF5340, SX1262, DW3110, HX711x2, BMI270, VL53L5CX, brake drivers, encoders
Text Notes 1000 1500 0 50 ~ 0
U1 nRF5340 I2C->BMI270/VL53L5CX/DRV2605L; SPI->SX1262 and DW3110
Text Notes 1000 2000 0 50 ~ 0
J2/J3 hall encoders to timer capture, J4/J5 brake coils via BTS7960 bridges
Text Notes 1000 2500 0 50 ~ 0
J6/J7 handle load cells -> HX711A/HX711B -> GPIO data/clock
$EndSCHEMATC

EESchema Schematic File Version 7
EELAYER 0 0
EELAYER END
$Descr A4 11700 8268
encoding utf-8
Sheet 1 1
Title "WellSync Watershed Weather Sentinel"
Date "2026-09-04"
Rev "1.0"
Comp "Devices"
Comment1 "RP2040 + SX1262 + BME280 + soil probes"
Comment2 "Runoff context, freeze risk, dry-well forecast signals"
Comment3 "Rails: solar, LiFePO4, 3V3"
Comment4 "Buses: SPI, I2C, 1-Wire"
$EndDescr
$Comp
L Device:U U1
U 1 1 70000041
P 4700 2600
F 0 "U1" H 4700 3450 50 0000 C CNN
F 1 "RP2040" H 4700 3350 50 0000 C CNN
1 4700 2600
1 0 0 -1
$EndComp
Text Notes 1200 6900 0 50 ~ 0
Inputs:
Tipping bucket interrupt
BME280 I2C environmental sensor
3-depth soil probes via ADC mux
DS18B20 freeze-depth probe
INA219 solar/battery telemetry
$EndSCHEMATC

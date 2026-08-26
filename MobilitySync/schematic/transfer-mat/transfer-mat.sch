EESchema Schematic File Version 4
LIBS:mobilitysync-transfer-mat
$Descr A3 16535 11693
Title "MobilitySync Transfer Mat"
Date "2026-08-26"
Rev "A"
Comp "Nous Research / Devices"
$EndDescr
Text Notes 1000 1000 0 50 ~ 0
Main blocks: 12V input, LiFePO4 backup option, ESP32-S3, IWR6843AOP UART, ADS7953 SPI ADC, CD74HC4067 mux bank, NAU7802 load-cell front-end, SX1262
Text Notes 1000 1500 0 50 ~ 0
FSR matrix rows/cols through mux bank to ADC channels for 16x16 seat-pressure readout
Text Notes 1000 2000 0 50 ~ 0
Load cells at corners -> NAU7802, local buzzer and haptic motor for pacing cues
$EndSCHEMATC

# Utility Sentinel schematic notes

12 V fused input feeds reverse-polarity MOSFET then TPS5430. STM32 PA0 pressure, PA1 CT, PB6 flow, PB7/PB8 leak loops. PA8 drives TLP291 dry contact. Key PB0 and STOP PB1 must gate the output in hardware as well as firmware. SX1262 SPI1 PA5/PA6/PA7, PA4 NSS, PB5 DIO1. Separate SELV and any field wiring; installation by qualified personnel.

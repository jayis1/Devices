# Air sentinel connections (not a KiCad schematic)

STM32L072KZ concept: SPI2 PB13/PB15/PB14 to SX1262 SCK/MOSI/MISO; PB12 CS, PA0 BUSY, PA1 DIO1, PA2 RESET. I²C PB6/PB7 to SHT45 0x44 and BME280 0x76, 3.3 V pullups. Add SWD pads, 100 nF decoupling at each IC, ESD/TVS at external connectors, and a tested Li-ion solar charger plus protected cell before use. Confirm pin alternate functions for the purchased package using ST datasheet and CubeMX. Not fabricated/validated.

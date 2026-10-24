# Surface probe connections (not a KiCad schematic)

STM32L072KZ SPI1 PA5/PA6/PA7 to MAX31865 SCK/MISO/MOSI; PA4 chip-select. Use PT1000 3-wire configuration with reference resistor and route sense lines as a matched, low-noise pair. Wetness electrode excitation must be current-limited and pulsed to reduce corrosion; divider output to PA0 ADC, protected against ESD. Radio shares SPI2 mapping in air-sentinel doc. Connector and probe insulation must be reviewed for outdoor use. Not fabricated/validated.

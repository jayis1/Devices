# Smart Dispenser connection notes

STM32G0B1 PB6/PB7 connects to HX711 DT/SCK and a 5 kg load cell. Pump microswitch enters PA0 with pull-up and debounce. TMP117 shares I2C1 PB8/PB9. SX1262 uses SPI1 PA5/PA6/PA7, PA4 NSS, PB5 DIO1. A 5 V/1 A SELV input feeds 3.3 V logic through a buck; manual pump mechanics do not depend on firmware.
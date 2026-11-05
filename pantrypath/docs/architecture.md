# Architecture / pin contract

Hub: Pi Zero 2 W 5 V USB; RFM95W SPI MOSI 10/MISO 9/SCLK 11, CS 8, reset 25, DIO0 24. Sentinels: ESP32-C3 3.3 V logic; I2C SDA GPIO6/SCL GPIO7; TMP117 address 0x48; reed input GPIO4 with pull-up. Shelf load cell feeds NAU7802 over I2C. Intake: RP2040 UART0 scanner TX/RX GPIO0/1, encoder GPIO14/15. Radios must use region-legal band/module/antenna. Board pin mapping must be checked against exact module datasheets before fabrication.

USB 5 V prototype power only. Board design needs fused input, reverse polarity and transient protection, regulated 3.3 V sized for radio burst, brownout monitoring. Battery version deferred pending cell/charger qualification and measured current budget. No mains, refrigeration control, emergency, or food-safety function. Local operation remains without internet; hub loss suspends aggregation while normal inspection continues. Native schematics and ERC are outstanding.

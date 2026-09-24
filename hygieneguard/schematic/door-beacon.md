# Door Beacon connection notes

nRF52840 P0.13/P0.14/P0.15 drive e-paper SPI SCK/MOSI/CS with P0.16 DC. Voluntary button uses P0.11 with internal pull-up. SX1262 uses P0.20 MOSI, P0.21 MISO, P0.22 SCK, P0.23 NSS, P0.24 DIO1. CR2477 feeds a low-quiescent 3.0 V regulator; do not route the battery under the antenna.
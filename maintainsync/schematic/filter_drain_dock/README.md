# Filter & Drain Dock schematic notes

12 V input → MP1584 5 V → AP2112K 3.3 V. ESP32-C6 I²C GPIO6/7 connects SDP810 and SHT45; float switches use GPIO2/3 with pull-ups and RC filtering; service button GPIO9; WS2812 GPIO8. SX1262: GPIO11 MOSI, 13 MISO, 12 SCK, 10 NSS, 4 DIO1. Use approved pressure ports and a drip loop for all wiring.

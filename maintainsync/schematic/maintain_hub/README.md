# Maintain Hub schematic notes

CM4 carrier: 5 V from TPS2115A, 3.3 V 3 A buck. S3 SPI routes to SX1262: MOSI 11, MISO 13, SCK 12, NSS 10, DIO1 4, BUSY 5, RESET 6. I²C GPIO8/9 routes ATECC608B and SHTC3. Keep SX1262 antenna and CM4 Wi-Fi antenna clear of copper; place 100 nF at every IC rail and 10 uF per regulator.

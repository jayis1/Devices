# Workshop Hub conceptual schematic

Author: jayis1. Conceptual connection notes, not fabrication-ready KiCad.

5 V USB-C and 6.4 V LiFePO4 UPS feed TPS2115A then a 3 A eFuse. CM4 carrier has 5 V input; ESP32-S3 has a 3.3 V buck. S3 GPIO11/13/12/10/4/5/6 connect SX1262 MOSI/MISO/SCK/NSS/DIO1/BUSY/RESET. E-paper CS/DC/RST/BUSY is GPIO15/16/17/18. Include USB-C CC resistors, ESD, decoupling, SWD/UART test pads, and radio antenna keep-out.
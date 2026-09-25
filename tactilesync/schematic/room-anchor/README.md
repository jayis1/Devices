# Room Anchor schematic reference

- U1 nRF52840-QIAA with Nordic RF reference network.
- U2 Qorvo DW3000 UWB module: SPI SCK=P0.13, MOSI=P0.15, MISO=P0.14, CS=P0.12, IRQ=P0.24, RST=P0.25.
- B1 two AA lithium cells -> U3 TPS62743 3.3 V low-IQ buck; 10 µH, 10 µF output.
- SW1 physical pair/reset on P0.11; LED1 on P0.10 through 1 kΩ.

Place UWB and BLE antennas at different board edges with no copper beneath their keep-outs.

# Haptic Band schematic reference

- U1 nRF52840-QIAA, 3.3 V, 32 MHz crystal and RF matching per Nordic reference.
- U2 DRV2605L: SDA=P0.26, SCL=P0.27, EN=P0.08, 10 µF local bulk; ERM/LRA output to J1.
- B1 protected 180 mAh LiPo -> U3 MCP73831 charger (USB-C 5 V) -> U4 AP2112K-3.3.
- SW1 acknowledge switch: P0.11 with 10 kΩ pull-up; SW2 quiet switch: P0.12.
- R1/R2 1 MΩ/330 kΩ divider from battery to P0.04 ADC, switched by P0.05.

Use a certified protected cell and verify thermal limits in the final enclosure.

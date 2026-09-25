# Appliance Marker schematic reference

- B1 CR2477 coin cell -> U1 TPS22916 load switch -> 3V3 haptic burst rail.
- U2 nRF52840-QIAA with vendor BLE RF matching; quiescent rail remains direct from B1.
- U3 DRV2605L, I²C SDA=P0.26/SCL=P0.27, enabled only during haptic pulses.
- SW1 sealed tactile button P0.11, 10 kΩ pull-up; optional capacitive pad connects through ESD diode.

Do not mount on hot, moving, or electrically energized appliance surfaces without appropriate isolation and manufacturer approval.

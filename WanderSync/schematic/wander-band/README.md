# WanderSync Wander Band — Schematic

## MCU: nRF52840 QFAA

- Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM
- Ultra-low-power (7-day battery on 300 mAh)
- BLE 5.0 (phone pairing, caregiver proximity)
- Sub-GHz via external SX1262

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, 2+ km range (critical for wandering tracking)
- SPI interface (MOSI=P0.12, MISO=P0.13, SCK=P0.14, NSS=P0.15)
- DIO1 interrupt (P0.16), RST (P0.17), BUSY (P0.18)

## GPS: Quectel L80-R

- 33-channel GPS, -162 dBm sensitivity, 1 Hz fix
- UART interface (P0.03 TX, P0.04 RX, P0.05 PPS)
- GPS enable on P0.06 (duty-cycle for battery: 1 Hz outdoor, 0.1 Hz indoor)
- Internal mini patch antenna (25×25 mm)

## Sensors

| Component | Interface | Pins |
|-----------|-----------|------|
| LSM6DSL IMU | I²C | P0.08 (SDA), P0.09 (SCL) |
| MAX30101 PPG | I²C | P0.10 (SDA), P0.11 (SCL) — via TCA9548A mux |
| DRV2605L haptic | I²C (separate bus) | P0.19 (SDA), P0.20 (SCL) |

## Other

| Component | Interface | Pin |
|-----------|-----------|-----|
| SOS button | GPIO (active low) | P0.21 |
| SK6812 LED | GPIO | P0.22 |
| Tamper switch (band removal) | GPIO | P0.23 |
| Battery voltage | ADC | P0.24 |
| USB-C power detect | GPIO | P0.25 |
| GPS fix LED | GPIO | P0.26 |
| MCP73831 charge status | GPIO | P0.27 |

## Power

- LiPo 3.7V 300 mAh (7-day life with duty-cycled GPS + Sub-GHz)
- MCP73831T USB-C charger (magnetic charging dock)
- TPS25940 eFuse for overcurrent protection

## Antennas

- Mini patch antenna for GPS (internal, 25×25 mm)
- PCB trace antenna for Sub-GHz 868 MHz (internal)
- PCB trace antenna for BLE 2.4 GHz (nRF52840 internal)

## Enclosure

- 3D-printed PA12, 42 × 32 × 14 mm
- IP67 water-resistant (showering, handwashing)
- Silicone wristband with tamper-resistant clasp
- Magnetic charging dock (easy for person with dementia)

## KiCad Project

Open `wander_band.kicad_pro` in KiCad 7+. Schematic sheets:
1. `wander_band MCU.sch` — nRF52840 + decoupling + flash + BLE antenna
2. `wander_band Power.sch` — USB-C, MCP73831, LiPo 300 mAh, eFuse
3. `wander_band SubGHz.sch` — SX1262 + matching network + PCB antenna
4. `wander_band GPS.sch` — L80-R + patch antenna + enable circuit
5. `wander_band Sensors.sch` — LSM6DSL, MAX30101, TCA9548A, DRV2605L, SOS, tamper
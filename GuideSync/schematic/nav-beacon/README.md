# Nav Beacon — Schematic

## MCU: nRF52840 QFAA
- Cortex-M4F 64 MHz, BLE 5.0
- BLE advertising-only mode (ultra-low power)

## Power
- CR2032 coin cell (220 mAh, 12-18 month life)
- Direct 3V → AP2112K-3.3 LDO (nRF52840 runs at 1.7-3.6V)
- Battery voltage on P0.04 (AIN4) — checked hourly

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| Status LED | GPIO | P0.02 |
| Reed Switch (config) | GPIO (IRQ) | P0.03 |
| Battery Monitor | ADC | P0.04 (AIN4) |

## BLE Advertising
- Manufacturer-specific data: company ID 0x0059 (Nordic)
- UUID prefix: 0x47, 0x53, 0xBE, 0xAC (GuideSync beacon)
- Beacon ID: 16-bit unique
- Battery: 8-bit voltage
- Advertising interval: 500 ms

## Physical Design
- 4-layer PCB 35×35 mm disc, 12 mm thick
- 3D-printed ASA enclosure (UV-resistant, wall-mounted)
- 3M VHB adhesive backing
- Reed switch on PCB edge for magnetic config mode

## Configuration
1. Hold magnet near beacon → reed switch triggers config mode
2. Beacon becomes BLE connectable (GATT service for setup)
3. Mobile app connects, sets landmark name + x/y coordinates
4. Remove magnet → beacon returns to advertising mode

## KiCad Project
Open `nav-beacon.kicad_pro` in KiCad 7+. Schematic sheets:
1. `beacon MCU.sch` — nRF52840 + decoupling
2. `beacon Power.sch` — CR2032 holder + LDO
3. `beacon IO.sch` — LED + reed switch + battery monitor
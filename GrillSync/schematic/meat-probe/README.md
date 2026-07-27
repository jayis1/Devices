# Meat Probe Schematic

## Overview

The Meat Probe is a wireless, waterproof multi-probe thermometer using
4× MAX31855K Type-K thermocouple interfaces. Measures internal meat
temperature at 4 depths. Communicates with Grill Hub via BLE 5.0.

## SoC: nRF52840 QFAA

- Cortex-M4F @ 64 MHz
- 1 MB flash, 256 KB RAM
- BLE 5.0
- Ultra-low power

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| P0.02 | MAX31855 #1 CS | SPI0 CS | Probe tip thermocouple |
| P0.03 | MAX31855 #2 CS | SPI0 CS | Probe mid thermocouple |
| P0.04 | MAX31855 #3 CS | SPI0 CS | Probe surface thermocouple |
| P0.05 | MAX31855 #4 CS | SPI0 CS | Ambient thermocouple |
| P0.06 | MAX31855 SCK | SPI0 | Shared SPI clock |
| P0.07 | MAX31855 MISO | SPI0 | Shared SPI MISO |
| P0.08 | LED | SK6812 | Status indicator |
| P0.09 | VBAT | ADC | Battery voltage monitor |
| P0.10 | USB detect | GPIO | USB-C power detect |
| P0.11 | Button A | GPIO | Probe select / bind |
| P0.12 | Button B | GPIO | Calibration trigger |
| P0.13 | Charger status | GPIO | MCP73831 STAT |
| P0.14 | Probe EN | GPIO | Thermocouple power gate |
| P0.15 | BLE IRQ | GPIO | SoftDevice IRQ |
| P0.16 | Temp alert | GPIO | Over-temp interrupt |

## Power

- **Battery:** LiPo 3.7V 500 mAh
- **Charger:** MCP73831 (USB-C, 100 mA)
- **Runtime:** 8 hours (active cook, 2 Hz sampling, BLE)
- **Charge time:** 90 minutes

## Block Diagram

```
┌─────────────────────────────────────────┐
│           nRF52840 QFAA                 │
│  ┌──────┐ ┌──────┐ ┌──────┐            │
│  │Cortex│ │ BLE  │ │ Radio│            │
│  │ -M4F │ │ 5.0  │ │      │            │
│  └──┬───┘ └──┬───┘ └──┬───┘            │
│     │        │        │                 │
│  ┌──┴────────┴────────┴──┐             │
│  │   GPIO / SPI / ADC     │             │
│  └──┬─────┬─────┬────┬───┘             │
└─────┼─────┼─────┼────┼─────────────────┘
      │     │     │    │
   SPI0   ADC  GPIO  GPIO
      │     │     │    │
  ┌───┴──────────┐ ┌┴──┐ ┌┴──────┐
  │4× MAX31855K  │ │VBAT│ │Buttons│
  │(SPI CS×4)    │ └───┘ └───────┘
  └──┬───┬───┬───┘
     │   │   │
  ┌──┴┐┌─┴─┐┌──┴┐
  │TC1││TC2││TC3│  Type-K thermocouples
  └───┘└───┘└───┘  (tip, mid, surface, ambient)
       ┌───┐
       │TC4│
       └───┘
```

## Key Design Notes

1. **MAX31855K:** SPI interface, cold-junction compensated. 14-bit
   signed thermocouple temperature (0.25°C resolution), 12-bit internal
   reference (0.0625°C). Fault detection: open-circuit, short-to-GND,
   short-to-VCC.

2. **4 thermocouple depths:** The probe has 4 Type-K thermocouples at
   different positions along the shaft:
   - Tip: center of meat thickest part
   - Mid: halfway along insertion depth
   - Surface: at meat surface
   - Ambient: in air above grill grate

3. **SPI bus:** All 4 MAX31855 share SCK and MISO, with individual CS
   lines. Read rate: 2 Hz during active cook, 0.1 Hz idle.

4. **Moving average:** 5-tap moving average filter per thermocouple
   to reduce noise.

5. **IP67 enclosure:** Food-grade PTFE cable rated to 300°C for the
   thermocouple shaft. Electronics in separate IP67 housing.

6. **BLE GATT:** Custom service with telemetry (notify, 18 bytes) and
   config (write, 4 bytes: probe_id, meat_type, doneness, target) characteristics.

7. **Power management:** nRF52840 sleeps between readings (1 µA),
   wakes for 50 ms SPI reads. BLE advertising at 250 ms intervals
   during active cook, 1s idle.
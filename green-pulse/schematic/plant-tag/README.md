# Plant Tag Schematic — GreenPulse

## MCU Architecture

Single-MCU: **nRF52832** (Bluetooth SoC, but used for Sub-GHz via external SX1262). Ultra-low-power for 18-month coin-cell life.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              PLANT TAG (per pot)             │
                    │                                              │
  CR2477 3.0V ───►  │  nRF52832 (main MCU, 14µA deep sleep)       │
  (1 Ah, 18mo)      │  ├── ADC  ── Capacitive Soil Moisture (SMT100-style)
                    │  ├── I2C ── VEML7700 ambient light           │
                    │  ├── I2C ── SHT40 temp + humidity            │
                    │  ├── SPI  ── SX1262 Sub-GHz radio             │
                    │  └── GPIO ── LED + button + soil power gate  │
                    │                                              │
  SX1262 ─────────  │  Sub-GHz TX 868/915 MHz, +10 dBm             │
                    │  PCB trace antenna (compact, no external)    │
                    │                                              │
  Capacitive Soil ──│  Probe inserts into potting soil (2-prong)   │
  (power-gated)     │  Powered only during read (5ms) to save bat │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — nRF52832 (QFN48)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.00 | ADC AIN0 | Capacitive soil moisture analog |
| P0.01 | — | Reserved (could be 2nd soil probe) |
| P0.04 | I2C SDA | VEML7700 + SHT40 (shared bus) |
| P0.05 | I2C SCL | VEML7700 + SHT40 (shared bus) |
| P0.06 | GPIO | Soil sensor power gate (high during read) |
| P0.08 | GPIO | SX1262 CS (SPI) |
| P0.09 | GPIO | SX1262 RST |
| P0.10 | GPIO | SX1262 BUSY |
| P0.11 | GPIO | SX1262 DIO0 (IRQ) |
| P0.12 | SPI SCK | SX1262 SPI clock |
| P0.13 | SPI MOSI | SX1262 SPI MOSI |
| P0.14 | SPI MISO | SX1262 SPI MISO |
| P0.22 | GPIO | Status LED (pairing + low-batt) |
| P0.24 | GPIO | Pairing button (active low) |
| P0.31 | ADC | Battery voltage divider (1/2) |

## Power Architecture

- **CR2477 coin cell** (3.0V, 1 Ah) → 18+ months
- Sleep current: ~14 µA (nRF52832 System ON + RTC + RAM retention)
- Active current: ~25 mA for 200ms (soil read + I2C + Sub-GHz TX)
- Duty cycle: 1 read per 15 min = 9000 reads/year
- Annual energy: 9000 × 200ms × 25mA = 1.25 Ah/year → ~18 months

## Soil Moisture Sensor

- **Capacitive** (not resistive) — won't corrode
- Principle: capacitance changes with soil dielectric (water content)
- Output: analog voltage 0-3V → ADC → VWC %
- Calibration: dry soil = ~3.0V → 0%, saturated = ~1.5V → 100%
- Power-gated: only energized for 5ms per read to eliminate DC leak

## Sub-GHz Antenna

- PCB trace antenna tuned for 868/915 MHz
- Compact λ/4 design: ~82mm trace (868 MHz) or ~77mm (915 MHz)
- Matched with π-network to 50Ω SX1262 output
- Radiation: omnidirectional in plane of PCB

## Physical Design

- PCB: 35×15mm 2-layer FR4 (probe board + radio board stacked)
- Soil probe: 100mm × 15mm tines inserted into soil (gold-plated pads)
- Enclosure: waterproof IP65 pot-stake housing (120mm tall)
- Weight: ~20g including coin cell
- Mount: simply push into soil next to plant stem
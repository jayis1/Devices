# PowerPulse Solar Node Schematic

## Overview
The solar node implements a 600W MPPT charge controller for 48V LiFePO4 battery banks, monitors cell health via CAN-connected BMS, and reports production data to the hub via Sub-GHz radio. The RP2040's PIO makes it ideal for generating the precise complementary PWM needed for synchronous buck conversion.

## MPPT Buck Converter Design
- **Topology**: Synchronous buck (non-isolated)
- **Input**: 10-35V DC from solar panel (typical: 30V)
- **Output**: 40-58V DC for 48V LiFePO4 battery (typical: 56V bulk)
- **Max power**: 600W (20A @ 30V input)
- **Switching frequency**: 100 kHz
- **Dead time**: 50ns (set by PIO program)
- **Inductor**: 47µH, 30A saturation (Coilcraft)
- **Input capacitors**: 2× 100µF/50V electrolytic + 4× 100nF ceramic
- **Output capacitors**: 2× 470µF/63V electrolytic + 4× 100nF ceramic

## MOSFET Selection
- **High-side**: IRFP4468 (100V, 2.6mΩ, 195A, TO-247)
- **Low-side**: IRFP4468 (synchronous rectification)
- **Gate driver**: IR2104 half-bridge driver with 50ns dead time
- **Gate resistors**: 10Ω series + 100kΩ pull-down

## Safety Features
- **Emergency shutdown** pin (GPIO27) — hardware-forced duty cycle to 0%
- **Overvoltage protection** — software shuts down if PV > 35V or battery > 58V
- **Overcurrent protection** — INA260 monitors current, software limits to 20A
- **Overtemperature** — MAX31855 + K-type thermocouple on heatsink, fan ramps 60-80°C, shutdown at 90°C
- **Reverse polarity** — diode on solar input (1N5408 or Schottky)

## RP2040 PIO for MPPT PWM
The RP2040's PIO is perfect for generating complementary PWM with dead time:
- PIO State Machine 0 generates high-side and low-side gate signals
- Complementary outputs on GPIO10 (high) and GPIO11 (low)
- 50ns dead time between transitions prevents shoot-through
- Duty cycle pushed from TX FIFO (can be updated every cycle)

## KiCad Project
Open `solar-node.kicad_pro` in KiCad 7+.

**⚠️ WARNING: This board handles high current (up to 20A) and battery voltages (up to 58V). Proper thermal management and fusing are mandatory.**
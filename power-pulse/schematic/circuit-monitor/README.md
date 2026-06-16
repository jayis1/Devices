# PowerPulse Circuit Monitor Schematic

## Overview
The circuit monitor installs inside the breaker panel and measures per-circuit current, voltage, and power factor using clamp-on CT sensors. It runs real-time arc fault detection using spectral analysis on the STM32G474's DSP-capable Cortex-M4F core.

## Safety-Critical Design Notes
- **GALVANIC ISOLATION IS MANDATORY**: The ISO7741 digital isolators create a complete isolation barrier between the high-voltage CT/voltage input side and the MCU side. Never bridge this barrier.
- **HLK-PM03** is a UL-recognized AC-DC converter with reinforced insulation (3kV isolation)
- **CT sensors (SCT-013-030)** are non-invasive — they clamp around wires without cutting them
- **All CT inputs** have 3.3V clamp diodes (BAT54S) and 1kΩ series resistors for overvoltage protection
- **The voltage sense transformer** is a small potential transformer (240V:9V) providing isolation

## ADS131E08 ADC Configuration
- 8 channels, 24-bit resolution
- Sampling rate: 8 kSPS per channel
- Input range: ±2.5V (internal VREF)
- Simultaneous sampling on all channels
- Two ADS131E08 chips for 16 channels total (chip 0: channels 0-7, chip 1: channels 8-15)
- MUX configuration: channel 0 = voltage reference, channels 1-7 = CT inputs (chip 0), channels 0-7 = CT inputs (chip 1)

## KiCad Project
Open `circuit-monitor.kicad_pro` in KiCad 7+.

**⚠️ WARNING: This board connects directly to AC mains. Only qualified electricians should install it. Always de-energize the panel before installation.**
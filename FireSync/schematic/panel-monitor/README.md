# Panel Monitor — Schematic

## MCU: STM32G431CBU6

- Cortex-M4F 170 MHz, 128 KB flash, 32 KB RAM
- Optimized for real-time signal processing (FFT)
- Hardware FPU for floating-point FFT

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI (MOSI=PB0, MISO=PB1, SCK=PB2, NSS=PB3)
- DIO1 (PB4), RST (PB5), BUSY (PB6)

## Current Sensing (SCT-013 CT Clamps)

| Component | Interface | Pins |
|-----------|-----------|------|
| SCT-013-100 CT clamp L1 | ADC1_IN1 | PA0 |
| SCT-013-100 CT clamp L2 | ADC1_IN2 | PA1 |
| AC voltage divider | ADC1_IN3 | PA2 |

- 8 kHz simultaneous sampling via ADC + DMA + TIM3 trigger
- 100A CT clamps, non-invasive (clamp around main breaker feeds)
- Burden resistors: 100Ω (100A→50mA→5V at full scale)
- ADC: 12-bit, 0-3.3V, DMA circular buffer (3 channels × 8000 samples)

## Thermal Sensors (DS18B20 ×4)

| Sensor | Location | Pin |
|--------|----------|-----|
| DS18B20 #1 | Main bus bar | PA4 (1-Wire) |
| DS18B20 #2 | Neutral bar | PA5 |
| DS18B20 #3 | Hottest breaker (L1) | PA6 |
| DS18B20 #4 | Hottest breaker (L2) | PA7 |

## Shunt-Trip Relay

- Output: PB7 (GPIO push-pull)
- Drives relay coil → triggers shunt-trip breaker
- 500 ms pulse to trip, manual reset required
- Fails OPEN (trips on loss of control power)

## Power

- AC mains → isolated AC-DC converter (5V) → AP2112K-3.3 LDO
- LiPo 500 mAh backup (brief power blip coverage)
- Battery voltage on PB9 (ADC2)

## Signal Processing

- 2048-point FFT on current waveform (CMSIS-DSP arm_rfft_fast_f32)
- 8 kHz sampling → 0-4 kHz frequency range, 3.9 Hz/bin resolution
- ArcDetect CNN: 1D-CNN over FFT magnitude bins (4-class)

## Enclosure

- 3D-printed ABS UL94-V0 (flame-retardant)
- Panel-mounted inside breaker panel enclosure
- CT clamps install around main feeders (non-invasive)

## KiCad Project

1. `panel MCU.sch` — STM32G431 + decoupling + clock
2. `panel SubGHz.sch` — SX1262 + PCB antenna
3. `panel CT Clamps.sch` — SCT-013 ×2 + burden resistors + ADC front-end
4. `panel Thermal.sch` — DS18B20 ×4 + 1-Wire pull-ups
5. `panel Power.sch` — AC-DC isolated + LDO + LiPo
6. `panel Shunt.sch` — Relay driver + shunt-trip breaker interface
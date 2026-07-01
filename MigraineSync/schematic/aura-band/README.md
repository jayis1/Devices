# MigraineSync — Aura Band Schematic

## SoC: nRF52840 QFAA

### Power
- 200 mAh LiPo → TP4056 (USB-C charging) → 3.3V → nRF52840 + sensors
- Battery voltage divider → P0.04 (ADC) for battery monitoring
- Target: 1.5-day battery life with 25% PPG duty cycle

### I²C Bus (shared)
| Signal | nRF Pin |
|--------|---------|
| SDA    | P0.08   |
| SCL    | P0.09   |

### I²C Devices
| Device    | Address | Function                        |
|-----------|---------|--------------------------------|
| MAX30101  | 0x57    | PPG (green/red/IR) — HR/HRV/SpO₂ |
| TMP117    | 0x48    | Skin temperature ±0.1°C        |
| BMP390    | 0x76    | Barometric pressure (on-wrist) |
| VEML7700  | 0x10    | Ambient light (personal dose)  |
| LSM6DSO   | 0x6A    | 6-axis IMU (activity context)  |

### GPIO
| Function          | nRF Pin |
|-------------------|---------|
| MAX30101 INT1     | P0.06   |
| LSM6DSO INT1      | P0.07   |
| Button (mark)     | P0.15   |
| Vibrator (haptic) | P0.16   |
| LED (green)       | P0.13   |
| Battery ADC       | P0.04   |

### PPG Configuration
- MAX30101 mode: multi-LED (red + IR + green)
- Sample rate: 100 Hz
- Green LED current: 6.4 mA (adjustable per skin tone)
- Duty cycle: 25% (15s on, 45s off) to extend battery
- FIFO interrupt → P0.06

### Power Budget
| Component       | Active Current | Duty Cycle | Avg Current |
|----------------|---------------|------------|-------------|
| nRF52840 (BLE) | 8 mA          | 100%       | 8.0 mA      |
| MAX30101       | 1.2 mA        | 25%        | 0.3 mA      |
| BMP390         | 3.2 µA        | 100%       | 0.003 mA    |
| VEML7700       | 0.2 mA        | 50%        | 0.1 mA      |
| LSM6DSO        | 0.013 mA      | 100%       | 0.013 mA    |
| TMP117         | 0.0035 mA     | 100%       | 0.004 mA    |
| **Total**      |               |            | **~8.4 mA** |

Battery life: 200 mAh / 8.4 mA ≈ 24 hours

### PCB
- 4-layer rigid-flex, 40×30 mm
- Castellated holes for nRF52840 module
- PPG sensor on bottom side (skin contact)
- Antenna clearance: 5mm keep-out on edge

### BLE
- Custom service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- TX characteristic (notify): `6e400002-...`
- RX characteristic (write): `6e400003-...`
- Connection interval: 30 ms (low latency for alerts)
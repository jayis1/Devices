# MigraineSync — Hydrate Tag Schematic

## SoC: nRF52840 QFAA

### Power
- CR2032 3V 220 mAh (primary cell, 6-month target life)
- Optional: 110 mAh LiPo with TP4056 (alternative power)
- Average current: ~0.3 mA (sleep 6 µA + active bursts)

### I²C Bus
| Signal | nRF Pin |
|--------|---------|
| SDA    | P0.08   |
| SCL    | P0.09   |

### I²C Devices
| Device    | Address | Function                        |
|-----------|---------|--------------------------------|
| LSM6DSO   | 0x6A    | 6-axis IMU (tilt detection)   |

### HX711 Load Cell (Bit-Bang GPIO)
| Signal  | nRF Pin |
|---------|---------|
| SCK     | P0.04   |
| DOUT    | P0.05   |
| RATE    | P0.16   |

### GPIO
| Function           | nRF Pin |
|--------------------|---------|
| LSM6DSO INT1       | P0.06   |
| LED (blue)         | P0.11   |
| Buzzer             | P0.13   |
| Button (manual)    | P0.15   |
| HX711 SCK          | P0.04   |
| HX711 DOUT         | P0.05   |
| HX711 RATE         | P0.16   |

### Load Cell Configuration
- HX711 gain: 128 (channel A)
- Sample rate: 10 Hz (RATE pin = GND)
- Load cell: 5 kg strain gauge bar type
- Calibration: `scripts/calibrate_sensors.py --hydrate /dev/ttyUSBx`
- Power management: HX711 enters power-down when SCK held high >60µs
- Wake: LSM6DSO tilt interrupt → power up HX711 → sample 2s → sleep

### Power Budget
| Component       | Active Current | Duty Cycle | Avg Current |
|----------------|---------------|------------|-------------|
| nRF52840 (sleep)| 6 µA          | 95%        | 5.7 µA      |
| nRF52840 (BLE)  | 8 mA          | 2%         | 0.16 mA     |
| HX711           | 1.4 mA        | 1%         | 0.014 mA    |
| LSM6DSO         | 13 µA         | 100%       | 13 µA       |
| **Total**       |               |            | **~0.19 mA**|

Battery life: 220 mAh / 0.19 mA ≈ 1158 hours ≈ 48 days
(conservative — actual with more BLE activity: ~6 months with light use)

### PCB
- 2-layer FR4, 25 mm disc
- Load cell mounted externally via 4-wire cable
- Castellated holes for nRF52840 module
- Antenna clearance: 5mm keep-out

### Form Factor
- Silicone sleeve fits standard water bottles (28 mm neck)
- Load cell pendant hangs below bottle base
- PCB disc sits on bottle side
- Waterproof: IP54 (splash resistant, not submersible)
# Door Tag — Schematic

## Block Diagram

```
┌────────────────────────────────────────────────────────────┐
│                   DOOR TAG (nRF52840)                      │
│                                                            │
│  ┌─────────────┐  ┌──────────┐  ┌──────────────────┐      │
│  │ nRF52840    │  │ CR2032   │  │ SPH0641LU4H-1    │      │
│  │ QFAA        │  │ Battery  │  │ I²S MEMS Mic     │      │
│  │ 1MB Flash   │  │ 3V       │  │ (ring detection) │      │
│  │ 256KB RAM   │  │ 220mAh   │  │                  │      │
│  │ BLE 5.0     │  └────┬─────┘  └────────┬─────────┘      │
│  └──────┬──────┘       │                  │ I²S            │
│         │              │ LDO              │                │
│         │              │ (internal)       │                │
│  ┌──────┴──────┐  ┌────┴─────┐   ┌────────┴────────┐      │
│  │ Piezo Disc  │  │ Power    │   │ Mic Enable MOSFET│     │
│  │ 35mm        │  │ Gate     │   │ (duty-cycled)    │     │
│  │ Contact     │  │          │   └──────────────────┘     │
│  │ Sensor      │  └──────────┘                            │
│  │ (knock/     │                                           │
│  │  doorbell)  │  ┌──────────────────────────────────┐    │
│  └──────┬──────┘  │ PCB Trace Antenna (BLE 5.0)      │    │
│         │ ADC     └──────────────────────────────────┘    │
│         │                                                   │
│  ┌──────┴──────┐  ┌──────────┐  ┌──────────┐            │
│  │ Comparator  │  │ SK6812   │  │ Buttons  │            │
│  │ (threshold │  │ RGB LED  │  │ (enroll) │            │
│  │  interrupt)│  │          │  │          │            │
│  └─────────────┘  └──────────┘  └──────────┘            │
└────────────────────────────────────────────────────────────┘
```

## Power Architecture

- **Battery:** CR2032 (3V, 220 mAh)
- **Regulation:** nRF52840 internal DCDC (2.1V core, 3.0V I/O)
- **Battery Life:** ~12 months
  - Piezo ADC sampling: 100 Hz, ~5µA average
  - Mic listening: 2 seconds every 30 seconds, ~2mA average
  - BLE advertising: 1-second interval, ~15µA average
  - Total average: ~25µA → 220mAh / 0.025mA = ~12 months
- **No charging:** CR2032 is replaceable

## Dual Detection Method

### 1. Piezo Contact Sensor (always-on, ultra-low-power)
- **Sensor:** 35mm piezo disc, adhered to door/phone surface
- **Detection:** Physical vibration → ADC sample (12-bit)
- **Threshold:** 2048/4096 (50% of full scale)
- **Interrupt:** Comparator wakes MCU from System OFF sleep
- **Knock Pattern:** 2+ knocks within 2-second window = door knock event
- **Doorbell:** Strong single vibration = doorbell mechanism strike

### 2. I²S MEMS Microphone (duty-cycled, 2s/30s)
- **Sensor:** SPH0641LU4H-1 I²S MEMS microphone
- **Detection:** Ring-tone pattern analysis (doorbell chime, phone ring)
- **Power:** MOSFET-gated (GPIO10), enabled only during listen window
- **Duration:** 2 seconds of audio every 30 seconds
- **Classification:** Simple FFT → frequency pattern matching for ring tones

## Key ICs

| IC | Function | Package | Interface |
|----|----------|---------|-----------|
| nRF52840 QFAA | Main MCU, BLE 5.0 | QFN-73 | — |
| SPH0641LU4H-1 | I²S MEMS microphone | LGA | I²S |
| Piezo disc 35mm | Contact vibration sensor | Disc | ADC |
| DMG1012UVW | Mic power MOSFET gate | SOT-523 | GPIO |
| SK6812 | RGB status LED | 5050 | GPIO (NRZ) |

## Pin Assignments

See `firmware/common/config.h` for complete pin assignments.

### I²S (microphone):
- BCLK: P0.02, LRCLK: P0.03, DATA: P0.04

### ADC:
- Piezo: P0.05 (12-bit ADC, 3V reference)
- VBAT: P0.07

### GPIO:
- LED: P0.06 (SK6812), Status: P0.09
- Button: P0.08 (enrollment trigger)
- Mic enable: P0.10 (MOSFET gate)

## Enclosure

- **Material:** 3D-printed PETG, adhesive-backed
- **Size:** 35mm × 25mm × 8mm
- **Mount:** 3M VHB adhesive (strong, removable)
- **Piezo contact:** Bottom exposed piezo disc contacts surface
- **Mic port:** 1mm hole on top for airborne sound
- **Battery:** CR2032 in rear compartment (toolless replacement)
# MedSync - Pill Station Schematic

## Overview

The pill station is a motorized medication dispenser with per-bin weight verification, IR beam-break detection, and motorized carousel.

## Block Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    PILL STATION PCB                               │
│                                                                   │
│  ┌──────────────┐    UART     ┌──────────────┐                   │
│  │  STM32F407   │◄──────────►│  nRF52832     │                   │
│  │  (Motor Ctrl │            │  (BLE Mesh    │                   │
│  │  + Sensors)  │            │   Router)     │                   │
│  │              │            │               │                   │
│  │  GPIO ──────►│ 8× A4988  │  BLE Antenna  │                   │
│  │  (STEP/DIR/  │ Stepper    │               │                   │
│  │   EN)        │ Drivers    │  PN532 NFC    │                   │
│  │              │            │  (optional)   │                   │
│  │  ADC ◄──────│ 8× HX711  │               │                   │
│  │              │ Load Cells │               │                   │
│  │              │            │               │                   │
│  │  GPIO ◄──────│ 8× IR Det │               │                   │
│  │              │ectors      │               │                   │
│  │              │            │               │                   │
│  │  I2C ──────►│ SSD1306   │               │                   │
│  │              │ OLED       │               │                   │
│  │  I2C ──────►│ DS3231 RTC│               │                   │
│  │              │            │               │                   │
│  │  PWM ──────►│ Piezo     │               │                   │
│  │              │ Buzzer     │               │                   │
│  └──────────────┘            └───────────────┘                   │
│                                                                   │
│  Power: USB-C 5V 3A → LM2596-3.3V → STM32 + nRF52832           │
│         + 18650 Lipo backup for MCU/sensors only                 │
│         + 5V direct to A4988 stepper drivers                      │
│                                                                   │
│  Mechanical: 8-compartment carousel, each with:                  │
│    - 28BYJ-48 stepper motor for pill dispensing                   │
│    - HX711 + 5kg load cell for weight verification              │
│    - IR emitter/detector for beam-break sensing                  │
│    - WS2812B RGB LED for status indication                        │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

## Key Design Notes

1. **Dual MCU**: STM32F407 handles motor control and sensor processing (real-time critical). nRF52832 handles BLE mesh communication (reliable, low-latency). UART at 115200 baud.

2. **Per-Bin Weight Verification**: 8 HX711 load cell amplifiers are multiplexed to reduce pin count. Common SCK line, individual DOUT lines.

3. **Carousel Motor**: Single 28BYJ-48 stepper rotates the carousel to position the correct bin. A4988 driver allows microstepping for smooth rotation.

4. **Per-Bin Steppers**: Each bin has its own 28BYJ-48 stepper for dispensing individual pills. All stepper motors are enabled/disabled to save power.

5. **Safety**: Magnetic reed switch detects cover opening. Emergency stop disables all motors. Watchdog timer ensures firmware doesn't hang with motors running.

## KiCad Project Files

See `pill-station.kicad_pro`, `pill-station.kicad_sch`, `pill-station.kicad_pcb` in this directory.

Note: KiCad project files are not yet generated. Use the pin assignments and BOM from the main README to create the schematic.
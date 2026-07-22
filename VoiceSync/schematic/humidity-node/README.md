# Humidity Node — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              ESP32-WROOM-32E                 │
│                                             │
│  GPIO4  ── SX1262 NSS (SPI CS)             │
│  GPIO5  ── SX1262 SCK                       │
│  GPIO18 ── SX1262 MISO                      │
│  GPIO19 ── SX1262 DIO1 (IRQ)               │
│  GPIO21 ── SX1262 RST                       │
│  GPIO22 ── SX1262 BUSY                      │
│  GPIO23 ── SX1262 MOSI                      │
│  GPIO14 ── SHT40 SDA (I²C)                  │
│  GPIO15 ── SHT40 SCL                        │
│  GPIO25 ── Humidifier Relay                  │
│  GPIO26 ── Fan Relay                          │
│  GPIO27 ── Ultrasonic TRIG                   │
│  GPIO32 ── Ultrasonic ECHO                   │
│  GPIO33 ── SK6812 LED                       │
│  GPIO34 ── Manual button (input only)       │
└─────────────────────────────────────────────┘
```

## Subcircuits

### SX1262 Sub-GHz Radio
- VSPI host bus at 8 MHz
- 868 MHz whip antenna via SMA connector
- Standard SX1262 configuration

### SHT40 (Temp/Humidity)
- I²C address: 0x44 (GPIO14/GPIO15)
- ±0.2°C, ±1.8% RH
- Precise humidity control for vocal health

### Ultrasonic Tank Level (HC-SR04)
- TRIG: GPIO27 (10µs pulse to trigger)
- ECHO: GPIO32 (pulse width = distance)
- Distance = pulse_width × 0.0343 / 2 cm
- Tank height known (e.g., 20 cm)
- Level % = (1 - distance/tank_height) × 100

### Humidifier Relay
- GPIO25 (active high)
- SRD-05VDC-SL-C relay
- Controls humidifier power
- Flyback diode across relay coil

### Fan Relay (Excess Humidity)
- GPIO26 (active high)
- SRD-05VDC-SL-C relay
- Controls exhaust fan for excess humidity (>65%)
- Flyback diode across relay coil

### Power
- USB-C 5V continuous power
- AMS1117-3.3 LDO
- No battery needed

### Status LED
- SK6812 RGB on GPIO33
- Green: humidity OK, Blue: humidifying, Red: tank empty

## PCB Layout Notes
- 2-layer board (40×40 mm)
- Relays: separate ground pour, away from RF section
- HC-SR04: connector on enclosure edge for sensor cable
- SHT40: air flow access, not enclosed
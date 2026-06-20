# Water Valve Schematic — GreenPulse

## MCU Architecture

Single-MCU: **ESP32-C6** (WiFi6 + Sub-GHz via SX1262). Controls latching solenoid valve, monitors flow sensor and pressure, reports to hub.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │           WATER VALVE NODE (per zone)        │
                    │                                              │
  12V DC ─────────►  │  12V → solenoid (latching)                 │
  (wall adapter)    │  12V → 3.3V LDO → ESP32-C6                  │
  + 18650 backup    │                                              │
                    │  ESP32-C6                                    │
                    │  ├── GPIO ── Latching solenoid open/close    │
                    │  ├── PCNT ── YF-S401 flow sensor             │
                    │  ├── ADC  ── MPX5010DP pressure              │
                    │  ├── SPI  ── SX1262 Sub-GHz radio            │
                    │  ├── GPIO ── Status LED                     │
                    │  └── WiFi ── (optional, if out of mesh range)│
                    │                                              │
  SX1262 ─────────  │  Sub-GHz RX from hub (watering commands)    │
                    │  868/915 MHz                                 │
                    │                                              │
  Solenoid ───────  │  12V latching, normally closed              │
                    │  Pulse open 50ms / close 50ms (no hold)     │
                    │  → no continuous power draw                  │
                    │                                              │
  Drip emitters ──  │  Adjustable per-plant drippers (0.5-2 L/hr) │
                    │  on downstream tubing                       │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO2  | Valve Open | Latching solenoid open pulse (MOSFET gate) |
| GPIO3  | Valve Close | Latching solenoid close pulse (MOSFET gate) |
| GPIO4  | Flow Pulse | YF-S401 hall-effect output → PCNT |
| GPIO5  | Pressure ADC | MPX5010DP analog output |
| GPIO6  | Status LED | Green=ok, Red=leak/error |
| GPIO7  | SX1262 CS | SPI select |
| GPIO8  | SX1262 DIO0 | IRQ (RX done) |
| GPIO9  | SX1262 RST | Reset |
| GPIO10 | SPI SCK | SX1262 |
| GPIO11 | SPI MOSI | SX1262 |
| GPIO12 | SPI MISO | SX1262 |
| GPIO13 | DIP SW 1 | Zone ID bit 0 |
| GPIO14 | DIP SW 2 | Zone ID bit 1 |
| GPIO15 | DIP SW 3 | Zone ID bit 2 (0-7 zones) |
| ADC1   | Battery | 18650 voltage via divider |

## Solenoid Driver

- **Latching solenoid** (normally closed, dual-coil): 50ms pulse to open, 50ms pulse to close. Zero current between pulses.
- Driven by 2× N-channel MOSFETs (AO3400) with flyback diodes.
- 12V supply from wall adapter (or 18650 boost converter for reservoir-fed).
- Advantage: unlike non-latching solenoids, no continuous power draw, ideal for battery backup.

## Flow Sensor (YF-S401)

- Hall-effect turbine flow meter
- Pulse output: ~5880 pulses/liter (F ≈ 98 × flow_rate_L/min Hz)
- ESP32-C6 pulse counter (PCNT) counts pulses with no CPU overhead
- Used to: confirm water delivery (liters), detect leaks (flow after valve close), detect empty reservoir (zero flow during open valve)

## Pressure Sensor (MPX5010DP)

- Differential pressure sensor (0-10 kPa)
- Measures water line pressure at valve inlet
- <1 PSI → reservoir empty or line blocked → refuse to open valve (safety)
- Also detects blocked drip lines (high pressure during open)

## Safety Features

1. **Auto-close after max duration (10 min):** regardless of command, valve closes. Prevents flood if mesh command is corrupted or lost.
2. **Empty reservoir protection:** if pressure <1 PSI at start, refuse to open + report GP_WATER_NO_FLOW.
3. **Leak detection:** 3 seconds after close, if flow >5 pulses → GP_ALERT_LEAK flag. The hub/app alerts the user.
4. **Boot-time close:** valve always closes at firmware boot, ensuring no stuck-open state after power loss/reboot.
5. **Latching design:** if power is lost, the valve stays in its last position (closed by default).

## Power

- 12V DC wall adapter (1A) → solenoid (12V) + 3.3V LDO → ESP32-C6
- 18650 2600mAh backup: keeps MCU alive during outage (valve needs 12V, won't water during outage)
- Valve solenoid: 12V, ~300mA for 50ms = 0.15 mAh per pulse (negligible)
- ESP32-C6 idle: ~5 mA, active: ~25 mA

## Physical Design

- PCB: 60×40mm 2-layer FR4
- Enclosure: IP65 waterproof junction box (100×70×50mm)
- Solenoid + flow sensor in-line with water tubing (quick-connect barbs)
- Mount: zip-tie to pipe or shelf near water source
- Tubing: 6mm OD vinyl to drip emitters
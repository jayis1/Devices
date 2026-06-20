# Light Node Schematic — CalmGrid

## MCU Architecture

Single SoC: **ESP32-C6** — handles BLE mesh communication, LED PWM control via TLC5973 driver, and TSL2591 ambient light feedback for closed-loop brightness.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │          LIGHT NODE                         │
                    │          (room, plugged)                    │
                    │                                            │
  12V/2A PSU ────►  │  Buck 5V ── 3.3V LDO ── ESP32-C6          │
                    │  12V direct ── TLC5973 ── LED strip        │
                    │                                            │
  ESP32-C6 ───────  │  PWM  ── TLC5973 (warm 2700K channel)      │
                    │  PWM  ── TLC5973 (cool 6500K channel)      │
                    │  I2C  ── TSL2591 (ambient lux feedback)    │
                    │  I2C  ── SSD1306 OLED (scene display)      │
                    │  GPIO ── WS2812 (status LED)               │
                    │  GPIO ── Button (manual override)           │
                    │  GPIO ── Wall switch detect                 │
                    │  BLE  ── mesh to hub                        │
                    │  WiFi ── cloud schedule sync                │
                    │                                            │
  LED Strip ──────► │  Tunable-white: 2700K + 6500K dual channel │
                    │  12V, 24W, 2m strip                        │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO1  | LED PWM WARM | TLC5973 warm channel (2700K) |
| GPIO2  | LED PWM COOL | TLC5973 cool channel (6500K) |
| GPIO3  | TLC5973 SCK | LED driver clock |
| GPIO4  | TLC5973 DATA | LED driver data |
| GPIO5  | TLC5973 LAT | LED driver latch |
| GPIO6  | TSL2591 SDA | ambient light feedback (I2C) |
| GPIO7  | TSL2591 SCL | ambient light feedback (I2C) |
| GPIO12 | OLED SDA  | SSD1306 I2C |
| GPIO13 | OLED SCL  | SSD1306 I2C |
| GPIO14 | WS2812    | status LED |
| GPIO15 | Button    | manual override / scene cycle |
| GPIO16 | Override SW| wall-switch override detect |

## LED Driver Architecture

```
  12V ──► TLC5973 ──► Warm LED string (2700K, 12W)
                    ──► Cool LED string (6500K, 12W)

  ESP32-C6 ──PWM──► TLC5973 (16-channel, 16-bit per channel)
                     │
                     ├── Channel 0-7:  Warm LEDs (2700K)
                     └── Channel 8-15: Cool LEDs (6500K)

  TSL2591 ──I2C──► ESP32-C6 (closed-loop lux feedback)
                    │
                    └── Adjusts PWM to maintain target lux
```

## Closed-Loop Brightness Control

The TSL2591 ambient light sensor provides closed-loop feedback:
1. Target lux is set based on current scene (e.g., 500 lux for work)
2. TSL2591 measures actual ambient lux
3. If ambient > target + 100: reduce LED brightness
4. If ambient < target - 100: increase LED brightness
5. Update every 5 seconds

This compensates for:
- Natural daylight contribution (don't over-illuminate)
- Other light sources in the room
- Seasonal daylight variations

## Circadian Schedule

| Time | Scene | Warm PWM | Cool PWM | Brightness | CCT |
|------|-------|----------|----------|------------|-----|
| 06:00-09:00 | Sunrise | 255→76 | 0→255 | 10%→80% | 2700K→5500K |
| 09:00-12:00 | Morning work | 32 | 255 | 90% | 5500K |
| 12:00-17:00 | Afternoon | 48 | 240 | 80% | 5000K |
| 17:00-20:00 | Sunset | 64→255 | 240→0 | 80%→40% | 5000K→2700K |
| 20:00-22:00 | Evening | 200 | 16 | 30%→10% | 2700K |
| 22:00-06:00 | Night | 10 | 0 | 3% | 2200K |

## Power

- 12V/2A external PSU → 5V buck (MP1584) for ESP32-C6
- 12V direct to TLC5973 → LED strip (up to 24W)
- Always plugged (no battery)
- LED strip: 2m tunable-white, 2700K + 6500K, 12W per channel

## Physical Design

- PCB: 50×40mm 4-layer FR4 (controller only — LED strip is external)
- Enclosure: 55×45×20mm ABS, inline with LED strip
- OLED on front, button on side, DC jack + LED strip connectors on back
- Status LED on front edge
- Heatsink on TLC5973 for thermal management
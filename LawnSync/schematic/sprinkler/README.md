# Sprinkler Controller Schematic

## Overview

8-zone irrigation valve controller with flow monitoring, pressure sensing, rain detection, and Sub-GHz mesh connectivity. Powered by 24VAC transformer (mains).

## MCU: ESP32-WROOM-32E

- Dual-core Xtensa LX6 @ 240 MHz
- 4 MB flash
- Wi-Fi 4 (2.4 GHz) — used for initial setup/debug, not primary connectivity
- Sub-GHz mesh via SX1262 is primary communication

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                   ESP32-WROOM-32E                           │
│                                                             │
│  GPIO4  ─── SX1262 NSS (SPI CS)                            │
│  GPIO5  ─── SX1262 SCK (SPI CLK)                           │
│  GPIO18 ─── SX1262 MISO                                    │
│  GPIO23 ─── SX1262 MOSI                                    │
│  GPIO19 ─── SX1262 DIO1 (IRQ)                              │
│  GPIO22 ─── SX1262 RST                                    │
│  GPIO21 ─── SX1262 BUSY                                   │
│  GPIO25 ─── Zone 1 relay driver (ULN2803 ch1)              │
│  GPIO26 ─── Zone 2 relay driver (ULN2803 ch2)              │
│  GPIO27 ─── Zone 3 relay driver (ULN2803 ch3)              │
│  GPIO14 ─── Zone 4 relay driver (ULN2803 ch4)              │
│  GPIO12 ─── Zone 5 relay driver (ULN2803 ch5)              │
│  GPIO13 ─── Zone 6 relay driver (ULN2803 ch6)              │
│  GPIO15 ─── Zone 7 relay driver (ULN2803 ch7)              │
│  GPIO2  ─── Zone 8 relay driver (ULN2803 ch8)              │
│  GPIO17 ─── Master valve relay driver                      │
│  GPIO34 ─── Flow meter pulse (input only)                  │
│  GPIO35 ─── Rain sensor tip (input only)                   │
│  GPIO36 ─── Pressure sensor analog (ADC1_CH0)              │
│  GPIO33 ─── Status LED (blue)                              │
│  GPIO32 ─── Buzzer (PWM)                                   │
│  EN    ─── TPL5010 watchdog reset                          │
│                                                             │
│  3V3   ─── Decoupling: 10µF + 0.1µF                       │
│  EN    ─── 10kΩ pullup + button to GND                     │
│  IO0   ─── 10kΩ pullup + button to GND (boot)              │
└─────────────────────────────────────────────────────────────┘
```

## Valve Driver Circuit

### ULN2803A Darlington Array
- 8 channels, 500 mA each
- Built-in flyback diodes for inductive loads
- Drives G5LE-14 DC5 relays (5V coil, 10A contacts)

### Relay Outputs
- 8× zone valve outputs (24VAC switched)
- 1× master valve output (24VAC switched)
- Each output:
  - G5LE-14 relay (10A @ 24VAC)
  - TVS diode (SMBJ58A) across relay contacts
  - MOV (V275LA4) across relay contacts
  - LED indicator (per zone)

### Soft-Start PWM
- GPIO uses PWM at 50% duty for 100 ms on valve activation
- Reduces water hammer and extends valve life
- Full on after 100 ms

## 24VAC Power Input

```
24VAC Transformer (40VA)
    │
    ├── Bridge rectifier (KBP206)
    ├── Filter cap: 470µF 50V
    ├── LM2596 buck → 5V/3A (for relays)
    ├── AMS1117-3.3 → 3.3V (for ESP32)
    └── 24VAC direct → relay commons (zone valves)
```

## Sensors

### Flow Meter — YF-S201
- Hall-effect sensor, 1–30 L/min
- Pulse output: ~30 pulses/Liter
- GPIO34 (input-only pin) with 10 kΩ pullup
- Interrupt on falling edge → count pulses → flow rate

### Rain Sensor — Tipping Bucket 0.2 mm
- Optolis TB-204 or equivalent
- Reed switch output, 0.2 mm per tip
- GPIO35 (input-only) with 10 kΩ pullup
- Interrupt → count tips → rainfall accumulation

### Pressure Sensor — MPX5700AP
- 15–115 kPa analog output (0.2–4.7V)
- GPIO36 (ADC1_CH0)
- Used for leak/over-pressure detection

## Safety Circuitry

### TPL5010 Nano-Watchdog
- External watchdog timer (independent of ESP32)
- Requires periodic strobe on GPIO (WAKE/MISO pin)
- If ESP32 hangs → TPL5010 pulses EN line → hard reset
- Timeout: 2 minutes

### Hardware Interlocks
- Master valve relay (normally closed) — shutoff for all zones
- If ESP32 loses power → all relays open → valves close
- Manual override button → pulls master valve relay low → shutoff
- Fuse: 2A slow-blow on 24VAC input

## Antenna

- 868 MHz whip antenna, SMA connector
- Mounted on enclosure exterior
- π-network matching per SX1262 datasheet

## Enclosure

- IP54 weatherproof (NEMA 4X equivalent)
- DIN rail mountable
- Gland fittings for valve wires (9-pair + sensor cables)
- Status LED visible through window
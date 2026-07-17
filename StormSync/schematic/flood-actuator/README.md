# Flood Actuator Schematic

## Overview

The Flood Actuator controls the motorized backflow preventer valve, backup pump relay, and audible alarm. It has an independent hardware float switch safety interlock that operates even if the MCU fails.

## SoC: ESP32-WROOM-32E

- Dual-core Xtensa LX6 @ 240 MHz
- 4 MB flash
- Sub-GHz mesh node

## Actuators

### Motorized Ball Valve (1½" NPT, 12V DC)
- Spring-return: fails to CLOSED position on power loss (safety!)
- Close: GPIO25 → relay → 12V motor (close direction)
- Open: GPIO26 → relay → 12V motor (open direction)
- Position feedback: dual reed switches (GPIO33, GPIO34)
- Timeout: 30 seconds max travel time

### Backup Pump Relay (30A SPST)
- GPIO27 → IRLZ44N MOSFET → G2RL-1A relay
- Controls 12V DC backup sump pump
- Thermal cutoff switch on pump motor (hardware protection)

### Audible Alarm (100 dB Piezo Siren)
- GPIO12 → MOSFET → PSA-24T08A siren
- Auto-silence after 30 minutes (noise ordinance)

## Safety Interlocks

### Float Switch (HARDWARE — independent of MCU)
- High-level float switch (NO reed)
- Wired directly to valve close relay + pump relay through discrete logic
- MCU monitors GPIO14 for status reporting + alarm activation
- **This path operates even if MCU is dead or firmware crashes**

### Manual Override Button
- GPIO35 (input-only)
- Local emergency: closes valve + starts pump + activates alarm
- 3-second debounce

### TPL5010 External Watchdog
- Independent reset supervisor (not on MCU)
- 1-minute heartbeat required or reset triggered

## Power

- **Primary:** 24VAC mains → LM2596S-5.0 → 5V → AP2112K-3.3 → 3.3V
- **Backup:** 12V 7Ah SLA battery (critical for valve/pump operation during outage)
  - MCP73871 float charger
  - Powers valve motor + backup pump relay + electronics
  - 24 hours autonomy including valve cycling + pump operation

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-WROOM-32E                          │
│                                                             │
│  GPIO4  ─── SX1262 NSS (SPI CS)                            │
│  GPIO5  ─── SX1262 SCK                                     │
│  GPIO18 ─── SX1262 MISO                                    │
│  GPIO23 ─── SX1262 MOSI                                    │
│  GPIO19 ─── SX1262 DIO1                                    │
│  GPIO22 ─── SX1262 RST                                     │
│  GPIO21 ─── SX1262 BUSY                                    │
│  GPIO25 ─── Valve close relay (MOSFET driver)              │
│  GPIO26 ─── Valve open relay (MOSFET driver)               │
│  GPIO27 ─── Backup pump relay (MOSFET driver)              │
│  GPIO14 ─── Float switch (input, also HW interlocked)      │
│  GPIO12 ─── Alarm siren (MOSFET driver)                    │
│  GPIO13 ─── Mains detect (input)                           │
│  GPIO32 ─── Battery voltage (ADC1_CH4)                     │
│  GPIO33 ─── Valve position: closed (reed switch)           │
│  GPIO34 ─── Valve position: open (reed switch, input only) │
│  GPIO35 ─── Manual override button (input only)            │
└─────────────────────────────────────────────────────────────┘

    Float Switch ──┬── GPIO14 (MCU monitor)
                   ├── Valve Close Relay (HW path)
                   └── Backup Pump Relay (HW path)

    Manual Override ── GPIO35 (MCU) + HW debounce
```

## PCB Notes

- 4-layer PCB: signal / GND / power / signal
- Relay traces: wide (30A for pump relay) — 2mm minimum
- Separate ground plane for high-current relay returns
- MOV + TVS on valve motor outputs (back-EMF protection)
- IP67 NEMA 4X enclosure wall-mounted near sump pit
- Fuses: 15A blade fuse on pump relay output, 5A on valve motor
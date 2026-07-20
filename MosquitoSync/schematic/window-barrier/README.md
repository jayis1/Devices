# Window Barrier — Schematic

## Block Diagram

```
┌───────────────────────────────────────────────┐
│           ESP32-WROOM-32E                      │
│                                               │
│  GPIO4  ── SX1262 NSS (SPI CS)               │
│  GPIO5  ── SX1262 SCK                        │
│  GPIO18 ── SX1262 MISO                       │
│  GPIO23 ── SX1262 MOSI                       │
│  GPIO19 ── SX1262 DIO1 (IRQ)                 │
│  GPIO22 ── SX1262 RST                        │
│  GPIO21 ── SX1262 BUSY                       │
│                                               │
│  GPIO25 ── DRV8833 AIN1 (close direction)    │
│  GPIO26 ── DRV8833 AIN2 (open direction)     │
│  GPIO27 ── DRV8833 nSLEEP (enable)           │
│  GPIO14 ── Reed switch (closed position)     │
│  GPIO12 ── Reed switch (open position)       │
│  GPIO13 ── Manual override button           │
│  GPIO32 ── Battery voltage (ADC1_CH4)       │
│  GPIO33 ── Motor current (ADC1_CH5)         │
│  GPIO34 ── Solar voltage (ADC, input only)  │
│  GPIO35 ── Status LED                        │
└───────────────────────────────────────────────┘
```

## Subcircuits

### DRV8833 Motor Driver
- Dual H-bridge, 1.5A per channel
- AIN1=GPIO25 (close direction: motor forward)
- AIN2=GPIO26 (open direction: motor reverse)
- nSLEEP=GPIO27 (low = sleep, high = active)
- VMOT: 12V from boost converter
- VCC: 3.3V logic supply
- Current sense: motor current → GPIO33 (ADC) for stall detection

### N20 Gear Motor
- 12V DC, 6mm stroke, 30 RPM
- Torque: 1.2 kg-cm
- Drives magnetic screen retract/extend mechanism
- Stall current: ~1.5A (detected via ADC for anti-pinch)

### Reed Switches (Position Feedback)
- Closed position: GPIO14 (NO reed switch)
- Open position: GPIO12 (NO reed switch)
- Magnet on screen frame, switch on window frame
- Pull-up resistors: 10k to 3.3V

### Manual Override
- GPIO13: momentary push button (input only, pull-up)
- Always works — even if node is offline
- Press: toggle open/close

### Power
- Battery: LiPo 3.7V 2000 mAh
- Solar: 2W 5V panel (trickle charge)
- MCP73871: LiPo charger
- LM2596: 3.7V → 12V boost (for motor)
- AMS1117-3.3: 3.3V for ESP32

### TPL5010 Watchdog
- External watchdog timer (independent of MCU)
- Pulse every 2 minutes → if MCU doesn't acknowledge, resets MCU
- GPIO: DONE pin → ESP32 output (kick the dog)

## Safety Interlocks
1. Motor stall: current > 1.5A → immediate stop (anti-pinch)
2. Limit switches: reed switches stop motor at open/closed positions
3. Manual override: physical button always functional
4. Battery < 20% → alert
5. Auto-open: 30 min after auto-close (allows ventilation)

## PCB Layout Notes
- 4-layer board (40×40 mm)
- Motor current: separate ground return path
- DRV8833: thermal pad on bottom, good copper pour
- Enclosure: IP54, window frame mount, 3D-printed PETG
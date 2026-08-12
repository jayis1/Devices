# WanderSync Door Sentinel — Schematic

## MCU: ESP32-C3-WROOM-02-N4

- RISC-V single-core 160 MHz, 4 MB flash
- Ultra-low-power (12-month CR123A battery life)
- Deep sleep + GPIO wake on door state change + TDMA slot wake

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI interface (MOSI=GPIO7, MISO=GPIO8, SCK=GPIO9, NSS=GPIO10)
- DIO1 interrupt (GPIO11), RST (GPIO12), BUSY (GPIO13)
- PCB trace antenna (internal, compact for door frame)

## Door/Window Sensor

- Coto 9001 glass reed switch on GPIO2 (pull-up + external pull-up)
- Magnetic contact: one part on door, one on frame
- ESP32-C3 wake-on-GPIO for instant door-open detection from deep sleep

## Motorized Deadbolt

- Yale Assure 2 motorized deadbolt body (4× AA battery powered)
- L298N H-bridge driver: GPIO3 (lock/IN1), GPIO4 (unlock/IN2)
- Lock position feedback: Coto 9001 reed switch on GPIO5 (deadbolt locked/unlocked)
- Lock/unlock: 500 ms H-bridge drive, then check position feedback
- Interior thumbturn always works (fire egress compliance — motorized bolt doesn't block manual unlock)

## Tamper Detection

- D2F-01 microswitch on GPIO6 (pull-up)
- Detects tampering with sentinel enclosure or lock mechanism
- Triggers immediate DOOR_ALERT with tamper flag

## Power

- 2× CR123A lithium (6V → HT7333 LDO → 3.3V)
- 12-month battery life: deep sleep (~10 µA) + TDMA wake every 4 s + reed switch wake
- Motorized deadbolt draws power only during lock/unlock (200 ms × ~2/day = negligible)
- Battery voltage on GPIO16 (ADC, voltage divider)

## Other

| Component | Interface | Pin |
|-----------|-----------|-----|
| SK6812 LED | GPIO | GPIO14 |
| Buzzer 85 dB | PWM | GPIO15 |

## Enclosure

- 3D-printed ASA, door frame mount
- IP54, tamper-resistant screws (Torx T10 security)
- Compact: 45 × 30 × 18 mm

## KiCad Project

Open `door_sentinel.kicad_pro` in KiCad 7+. Schematic sheets:
1. `door_sentinel MCU.sch` — ESP32-C3 + decoupling
2. `door_sentinel Power.sch` — 2× CR123A, HT7333 LDO, voltage divider
3. `door_sentinel SubGHz.sch` — SX1262 + matching network + PCB antenna
4. `door_sentinel Lock.sch` — L298N H-bridge, deadbolt, position reed, tamper switch
5. `door_sentinel Peripherals.sch` — Reed switch, LED, buzzer
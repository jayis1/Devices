# PorchGuard lock-node Schematic

## KiCad Project

Open `lock-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB, 80×50mm (split interior escutcheon + exterior keypad)
- Impedance controlled: 50Ω for antenna (if PCB trace antenna used)
- Interior: motor + relay + MCU + battery
- Exterior: keypad + LEDs + buzzer (connected via ribbon cable)
- Conformal coating on exterior PCB

## Design Rules

- nRF52840 chosen for BLE 5.0 + ultra-low-power SystemOFF (<3µA)
- A4988 stepper driver with 1/2 microstep for smooth, quiet deadbolt throw
- Motor peak current ~250mA for <1s — 4×AA handles easily
- Relay isolated via PC817 optocoupler (garage opener circuits vary)
- Keypad: 3 columns × 4 rows = 12 capacitive touch keys
- LIS2DH12 on I2C with INT1 for tilt/shock wake (forced-entry detection)
- Reed switch on door frame for open/closed state
- SX1261 optional for Sub-GHz fallback when out of BLE range

## Key Design Considerations

- Motor power rail (6V direct from AA) separate from logic (3.3V via AP2112)
- Stepper enable held low only during drive (saves power, holds deadbolt mechanically otherwise)
- BLE advertising 100ms when active, 1s when idle — battery budget 8-12 months
- LE Secure Connections for encrypted phone/hub bonding
- One-time codes stored in flash (persist across power loss)
- Auto-lock re-engages 30s after unlock if door closed (configurable)
- Keypad anti-shoulder-surf: randomized digit-to-position mapping on each wake

## Pin Assignments (nRF52840)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.03 | GPIO | A4988 STEP |
| P0.04 | GPIO | A4988 DIR |
| P0.05 | GPIO | A4988 ENABLE (active low) |
| P0.06 | GPIO | A4988 MS1 (microstep) |
| P0.07 | GPIO | Garage relay |
| P0.08 | GPIO | Garage relay enable |
| P0.09 | EXTI | Reed switch (door state) |
| P0.10 | EXTI (wake) | LIS2DH12 INT1 (tamper) |
| P0.11 | EXTI | LIS2DH12 INT2 |
| P0.12 | GPIO/SPI CS | SX1261 CS |
| P0.13-15 | GPIO | SX1261 IRQ/BUSY/NRST |
| P0.20-22 | GPIO | Keypad cols 0-2 |
| P0.23-26 | GPIO | Keypad rows 0-3 |
| P0.30 | ADC | Battery voltage |
| P0.31 | GPIO | Status LED (RGB) |
| P1.00 | GPIO | Buzzer |

## Schematic Files

- `lock-node.kicad_sch` — Main schematic
- `lock-node.kicad_pcb` — PCB layout (work in progress)
- `lock-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/lock_node_bom.csv` for component details.
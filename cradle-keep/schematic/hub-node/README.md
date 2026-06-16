# CradleKeep hub-node Schematic

## KiCad Project

Open `hub-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Via size: 0.3mm drill, 0.6mm pad
- Impedance controlled: 50Ω for RF traces (SX1262)
- Ground plane on layer 2 (unbroken for RF performance)

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- RF section isolated from digital section
- Analog sensor inputs have RC filters (100Ω + 100nF)
- Power supply sequencing: 3.3V before 1.8V
- I2S audio traces: matched length, 50Ω impedance
- Speaker amp: PAM8302 with LC output filter for EMI suppression
- Battery charge: MCP73831 with 500mA charge current for 3000mAh LiPo
- USB-C: 5V 2A minimum for charging + operation

## Key Design Considerations

- RP2040 Core 0 runs mesh TDMA + safety rules + TFT (hard real-time)
- RP2040 Core 1 runs ESP32-C6 UART bridge (soft real-time)
- ESP32-C6 handles WiFi MQTT + BLE GATT server (no real-time requirement)
- SX1262 high-power PA (20dBm) for reliable whole-house mesh coverage
- PCM5102A I2S DAC drives 3W speaker for lullabies/white noise/alarms
- TFT display updates at 1Hz — low priority, never blocks safety checks
- Battery backup ensures mesh continues during power outage (critical for baby monitoring)

## Schematic Files

- `hub-node.kicad_sch` — Main schematic
- `hub-node.kicad_pcb` — PCB layout (work in progress)
- `hub-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/hub-node-bom.csv` for component details.
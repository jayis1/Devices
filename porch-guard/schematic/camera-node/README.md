# PorchGuard camera-node Schematic

## KiCad Project

Open `camera-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Impedance controlled: 50Ω for RF traces (SX1261)
- Camera bus routed on dedicated layer (8 data lines + sync)
- Ground plane unbroken under OV2640 + RF section
- IP65 enclosure, UV-resistant
- Conformal coating recommended for outdoor use

## Design Rules

- Decoupling caps within 1mm of ESP32-S3 + OV2640 power pins
- OV2640 XCLK from ESP32 (16MHz) — keep trace short
- mmWave UART at 256000 baud — direct to ESP32-S3 UART1
- PIR interrupt on GPIO1 (wakeup-capable) with 100nF debounce
- Tilt sensor on GPIO with IRQ wake
- Doorbell transformer 16-24VAC → MP1584 buck to 5V → AP2112-3.3V
- Supercap (1F) on 5V rail — survives 5s power dip (no missed clip on cut)
- White/IR LEDs PWM-driven for adaptive illumination

## Key Design Considerations

- ESP32-S3 with 8MB PSRAM variant for frame buffering + ML tensors
- PIR-gated capture: camera module powered down until PIR fires (<5µA sleep)
- mmWave presence enables sub-meter detection (distinguishes person vs pet)
- Two-way audio: INMP441 mic + MAX98357A speaker on separate I2S ports
- Clip pre-buffer kept in PSRAM (5s ring buffer) for instant clip save on event
- Sub-GHz mesh event channel works even if WiFi is down — pirate alert path
- Tilt sensor fires final TAMPER_ALERT via supercap if camera is moved/covered

## Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| 1 | GPIO (wakeup) | AM612 PIR motion |
| 3 | I2S | MAX98357A speaker (data) |
| 4-5 | I2S | INMP441 mic (WS, SCK) |
| 5 | I2S | INMP441 mic (SD) |
| 6 | I2S | INMP441 mic (SD) |
| 7-8 | I2S | MAX98357A (WS, SCK) |
| 9 | GPIO | SX1261 BUSY |
| 10 | GPIO/SPI | SX1261 CS |
| 11-13 | SPI2 | SX1261 (MOSI, MISO, SCK) |
| 14 | GPIO IRQ | SX1261 IRQ |
| 15-22 | Camera | OV2640 bus (PCLK, VSYNC, HREF, data, I2C, reset, pwdn, xclk) |
| 37 | GPIO | SD card CS |
| 38 | GPIO | White LED |
| 39 | GPIO | IR LED array |
| 40 | GPIO IRQ | SW-420 tilt |
| 41-42 | UART | HLK-LD2410 mmWave |
| 46 | GPIO | SX1261 NRST |

## Schematic Files

- `camera-node.kicad_sch` — Main schematic
- `camera-node.kicad_pcb` — PCB layout (work in progress)
- `camera-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/camera_node_bom.csv` for component details.
# WashWise scanner-node Schematic

## KiCad Project

Open `scanner-node.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- Minimum trace width: 0.15mm (signal), 0.5mm (power)
- Compact 60×40mm form factor (handheld)
- 50Ω controlled impedance for SX1261 RF (or PCB trace antenna)
- Ground plane on layer 2

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- Camera (OV2640): DVP parallel interface, short traces, matched length
- UV LED (365nm): driven at 350mA via constant-current driver
- IR LED (940nm): driven at 100mA via GPIO + current limiting resistor
- White LED: driven at 30mA via GPIO
- Display (ST7789): SPI interface, 4-wire SPI
- Capacitive touch: PCB pads with touch sensing via ESP32-S3 touch peripheral
- Battery: 1000mAh LiPo, MCP73831 charger (500mA), USB-C input
- Power: AP2112-3.3 LDO, deep sleep current < 5µA

## Key Design Considerations

- ESP32-S3 with 16MB flash + 8MB PSRAM (for image capture + ML model)
- OV2640 camera: parallel DVP interface (not SPI — faster for image capture)
- Three illumination LEDs positioned around camera lens for even lighting:
  - White LED: standard color/texture imaging
  - UV-A 365nm: biological stain fluorescence (protein, blood, sweat glow)
  - IR 940nm: oil/grease detection (oil absorbs IR → appears dark)
- TFLite Micro models stored in flash:
  - Fabric classifier: ~85KB (4-conv CNN)
  - Stain classifier: ~180KB (MobileNetV3-Small)
- Display: 1.3" 240×240 ST7789 (low power, crisp)
- Capacitive touch buttons on PCB (no physical buttons for waterproofing)
- PCB trace antenna for 868MHz (saves space vs SMA + whip)

## Pin Assignments (ESP32-S3)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO5-21 | DVP Parallel | OV2640 camera (8-bit data + HSYNC/VSYNC/PCLK/XCLK) |
| GPIO12-14 | SPI | SX1261 radio |
| GPIO15-18 | GPIO | SX1261 CS/BUSY/IRQ/NRST |
| GPIO35-36 | SPI | ST7789 display (SCK/MOSI) |
| GPIO37-40 | GPIO | Display CS/DC/RST/BL |
| GPIO41 | GPIO | White LED (via MOSFET) |
| GPIO42 | GPIO | UV-A 365nm LED (via constant-current driver) |
| GPIO2 | GPIO | IR 940nm LED |
| GPIO3 | Touch | Scan button |
| GPIO4 | Touch | Navigate button |
| GPIO5 | Touch | Confirm button |
| GPIO4 | ADC1_CH4 | Battery voltage |

## Multispectral Imaging Theory

| Spectrum | Wavelength | What it reveals |
|----------|-----------|-----------------|
| White | 400-700nm | Color, texture, visible stains |
| UV-A | 365nm | Biological stains fluoresce (blood, sweat, food) |
| IR | 940nm | Oil/grease absorbs IR (appears dark); fabric structure |

### Stain Discrimination by Spectra

| Stain | White | UV-A (365nm) | IR (940nm) |
|-------|-------|-------------|------------|
| Coffee/Wine | Dark | Dark (no fluorescence) | Medium |
| Blood | Dark red | Bright fluorescence | Dark |
| Grease/Oil | Translucent | Dark | Very dark |
| Grass | Green | Moderate fluorescence | Medium |
| Ink | Very dark | Dark | Dark |
| Sweat | Faint | Bright fluorescence | Medium |
| Clean | Fabric color | Low (baseline) | Fabric-dependent |

## Schematic Files

- `scanner-node.kicad_sch` — Main schematic
- `scanner-node.kicad_pcb` — PCB layout
- `scanner-node.kicad_pro` — KiCad project file

See BOM in `hardware/bom/scanner_node_bom.csv` for component details.
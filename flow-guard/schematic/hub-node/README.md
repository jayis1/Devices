# FlowGuard Schematic - Hub Node
# KiCad Project Placeholder

## MCU: nRF52840 + ESP32-C6

### Key Design Notes

1. **nRF52840** is the primary MCU running Zigbee 3.0 coordinator + local ML inference + TFT display
2. **ESP32-C6** handles WiFi6 uplink only — communicates with nRF52840 via UART at 1Mbps
3. Power path: USB-C → MCP73831 charger → 18650 Lipo → AP2112-3.3V → nRF52840
4. Separate AP6212-1.8V for ESP32-C6 core
5. TFT display on SPI1 (ILI9341, 320×240)
6. microSD card on SPI0 for data logging
7. MEMS microphone (SPH0645LM4H) on I2S for acoustic leak verification at hub
8. Hardware valve override signal: nRF52840 GPIO can directly signal valve controller via Zigbee or via dedicated GPIO line

### Power Sequencing
- nRF52840 boots first (Zigbee coordinator)
- ESP32-C6 held in reset until nRF52840 sends boot command via UART
- TFT display enabled after both MCUs are running

### RF Design
- nRF52840: PCB trace antenna (2.4GHz, matched to 50Ω)
- ESP32-C6: Built-in PCB antenna on module
- Antennas must be >20mm apart to avoid interference
- nRF52840 antenna on top edge, ESP32-C6 antenna on bottom edge of PCB

### Schematic Sheets
1. Power supply (USB-C, charger, regulators)
2. nRF52840 + debug header
3. ESP32-C6 WiFi bridge
4. Inter-MCU UART + control signals
5. SPI bus (TFT + SD card)
6. I2S MEMS microphone
7. User interface (button, buzzer, LEDs)
8. Expansion header (I2C, UART, GPIO)

### Net Classes
| Net Class | Width | Clearance | Via Size |
|-----------|-------|-----------|----------|
| Power | 0.5mm | 0.3mm | 0.8mm |
| Signal | 0.15mm | 0.15mm | 0.6mm |
| SPI | 0.2mm | 0.2mm | 0.6mm |
| RF | 50Ω coped | 0.2mm | N/A |

### KiCad Project Files
- `hub-node.kicad_pro` — Project file
- `hub-node.kicad_sch` — Schematic
- `hub-node.kicad_pcb` — PCB layout (110×70mm, 4-layer)

(These files would be generated in KiCad. This placeholder describes the design intent for schematic capture.)
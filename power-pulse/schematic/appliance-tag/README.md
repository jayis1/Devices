# PowerPulse Appliance Tag Schematic

## Overview
Small BLE mesh plug-level energy monitor and relay controller. Plugs into any wall outlet, measures per-appliance power, provides on/off control via solid-state relay, and reports data back to the hub through the BLE mesh network.

## Form Factor
- PCB: 45mm × 55mm, 2-layer
- Fits inside standard smart plug housing (pass-through type)
- Antenna: 2.4 GHz PCB trace antenna (nRF52840 module includes it)

## Key Design Decisions
- **nRF52840** for BLE 5.0 mesh with long-range mode (125 kbps at 200m+)
- **BL0937** for single-phase energy measurement — simple pulse-counting interface, no complex ADC needed
- **G3MB-202P** solid-state relay — zero-cross switching to minimize inrush current and EMI
- **SSD1306 OLED** — shows current power draw for immediate feedback without app
- **Single button** — short press toggles relay, long press (3s) enters BLE pairing mode

## Power Supply
- AC mains → HLK-PM01 (120/240VAC → 5V/600mA) → AMS1117 (5V → 3.3V/800mA)
- Total budget: ~50mA @ 3.3V (BLE active), ~15mA (sleep with mesh relay)
- SSR adds ~15mA when on

## Antenna Considerations
- The E73-2G4M08S1C nRF52840 module includes an integrated PCB antenna
- Keep antenna area clear of copper fills and components
- Ground plane under module for RF stability

## KiCad Project
Open `appliance-tag.kicad_pro` in KiCad 7+.
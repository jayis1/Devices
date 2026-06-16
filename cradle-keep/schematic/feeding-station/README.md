# CradleKeep feeding-station Schematic

## KiCad Project

Open `feeding-station.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal)
- 80mm × 60mm — countertop form factor
- Load cells mount to PCB via structural adhesive
- DS18B20 waterproof probe connects via 3-pin JST connector (cable exit)
- PTC heater mounts under stainless steel warming plate

## Design Rules

- All decoupling capacitors within 2mm of IC power pins
- HX711 ADC: keep analog traces short and shielded from digital
- PTC heater: IRLZ44N MOSFET with 10kΩ gate pull-down, 100kHz PWM
- DS18B20: 4.7kΩ pull-up on data line (1-Wire bus)
- Servo: 50Hz PWM, 1-2ms pulse width range
- Power supply: 5V USB-C → 3.3V AP2112 for nRF52840
- Heater runs directly on 5V USB-C (up to 3A for fast warming)

## Key Design Considerations

- nRF52840 chosen for native BLE (mobile app configuration)
- HX711 24-bit ADC provides 2g resolution on 5kg load cells
  - Combined resolution: 2 load cells × 5kg = 10kg total capacity
  - Effective resolution: ~1ml milk volume (milk density ~1.03 g/ml)
- PTC heater is self-limiting (resistance increases with temperature)
  - PID control maintains 37°C ±0.5°C for safe milk warming
  - Maximum warming time: ~4 minutes for 120ml from fridge temp
- DS18B20 probe sits in warming well beside bottle (not in milk)
- SG90 servo dispenser rotates 180° to release one scoop of formula
  - Adjustable scoop size for different formula brands
- Milk freshness check: VCSEL + photodiode turbidity measurement
  - Not a substitute for smell/taste — supplementary indicator only

## Schematic Files

- `feeding-station.kicad_sch` — Main schematic
- `feeding-station.kicad_pcb` — PCB layout (work in progress)
- `feeding-station.kicad_pro` — KiCad project file

See BOM in `hardware/bom/feeding-station-bom.csv` for component details.
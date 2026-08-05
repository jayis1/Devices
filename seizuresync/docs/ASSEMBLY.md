# SeizureSync — Assembly Guide

## Seizure Hub assembly

### PCB assembly
1. Order the 4-layer FR4 PCB (120×80 mm) from the Gerber files in `schematic/seizure-hub/`
2. Solder components per BOM (see `hardware/bom/SeizureHub_BOM.csv`)
3. Order: SoC → passives → connectors → sensors → radio → relays → display

### Critical notes
- **ESP32-S3-WROOM-1**: Ensure correct orientation (pin 1 marker)
- **SX1262**: Use 32 MHz crystal, keep RF trace to antenna short (≤ 20 mm)
- **MLX90640**: Mount with lens facing bed (thermal array FOV 75°)
- **Bed-mat piezo sensors**: Place 3× LDT0-028K under mattress at chest level,
  spaced 30 cm apart. Connect to hub via 3.5 mm jack or screw terminals.
- **12V SLA**: Use spade connectors; ensure correct polarity (LTC4040 has
  reverse-polarity protection but verify)
- **SIM7600G**: Insert SIM card before powering on; use external 4G antenna

### Enclosure
- 3D-printed ABS enclosure (140×100×40 mm) with STL in `hardware/`
- Ventilation slots for MLX90640 and SCD41
- Cable gland for USB-C power + bed-mat sensor cables
- Wall-mountable (2× keyhole slots on back)

### Setup
1. Plug in USB-C 5V/3A power
2. Hub boots, displays "SeizureSync Hub — Initializing..." on e-ink
3. Connect to Wi-Fi via mobile app (BLE pairing for initial config)
4. Insert SIM card for 4G LTE backup
5. Place bed-mat sensors under mattress
6. Hub displays "Monitoring active" — ready

## Seizure Band assembly

### PCB assembly
1. Order 4-layer flex-rigid PCB (45×35 mm)
2. Solder: ESP32-S3-MINI → SX1262 → ICM-42688-P → MAX30102 → AD5940 → DRV2605L
3. Solder LCD (FPC connector)
4. Solder USB-C connector + buttons

### Enclosure
- Watch case (44×38×14 mm), 3D-printed PC+ABS
- IP67 sealed (shower-safe)
- 20 mm silicone strap
- USB-C charging port with silicone cover

### Setup
1. Charge via USB-C (full charge ~2 h)
2. Pair with hub via mobile app (BLE)
3. Wear on wrist, snug fit (PPG sensor must contact skin)
4. Battery life: ~48 h between charges

## Aura Patch assembly

### PCB assembly
1. Order 2-layer FR4 flex PCB (35×25 mm)
2. Solder: nRF52840 → TMP117 → AD8232 → MAX30101
3. Solder CR2477 coin cell holder
4. Solder LED + button

### Enclosure
- Medical-grade silicone overmold (35×25×8 mm)
- 3M Tegaderm adhesive (replace every 14 days)
- IP68 sealed (shower/bath-safe)

### Setup
1. Remove backing, apply to left chest (over sternum)
2. Press button to confirm "worn" status (LED flashes green)
3. Pairs automatically with band/hub via BLE
4. Replace every 14 days (coin cell depleted)

## Caregiver Beacon assembly

### PCB assembly
1. Order 4-layer FR4 PCB (90×60 mm)
2. Solder: ESP32-C3 → SX1262 → e-ink → MAX98357A → DRV2605L → WS2812 matrix
3. Solder buttons (Acknowledge: green, 911: red recessed, Test: white)
4. Solder speaker + battery connector

### Enclosure
- 3D-printed ABS enclosure (100×70×35 mm)
- Belt clip + lanyard hole
- USB-C charging port

### Setup
1. Charge via USB-C (full charge ~3 h)
2. Pair with hub via mobile app (BLE config)
3. Carry throughout the home (Sub-GHz range ~500 m)
4. Battery life: ~7 days
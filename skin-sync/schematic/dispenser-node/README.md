# Smart Dispenser Schematic — SkinSync

## MCU Architecture

Single-MCU: **ESP32-C6** (WiFi6 + Sub-GHz receiver + pump control) + **SX1262** (Sub-GHz from hub).

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              SMART DISPENSER NODE            │
                    │              (countertop, 4 product slots)    │
                    │                                              │
  5V USB-C ───────  │  TP4056 ── 18650 backup ── 3.3V LDO         │
                    │                                              │
  ESP32-C6 ───────  │  GPIO/SPI/I2C/ADC ─── all peripherals       │
                    │  ├── I2C  ── SSD1306 0.96" OLED              │
                    │  ├── SPI  ── SX1262 (Sub-GHz from hub)       │
                    │  ├── GPIO ── 4× peristaltic pump drivers     │
                    │  │    (MOSFET gates, 3V pumps)               │
                    │  ├── GPIO ── 4× solenoid valve drivers       │
                    │  │    (normally-closed, anti-drip)           │
                    │  ├── GPIO ── 4× HX711 load cell amplifiers   │
                    │  │    (one per slot, weight tracking)        │
                    │  ├── I2C  ── MFRC522 RFID reader             │
                    │  │    (product cartridge identification)     │
                    │  ├── GPIO ── 4× manual dispense buttons      │
                    │  └── ADC  ── pump current monitor (fault)    │
                    │                                              │
  Peristaltic ────  │  4× micro-pumps (3V DC, 0.5ml/sec)          │
  pumps ×4           │  Slot 0: cleanser                           │
                    │  Slot 1: serum                               │
                    │  Slot 2: moisturizer                         │
                    │  Slot 3: sunscreen                           │
                    │                                              │
  Load cells ×4 ──  │  HX711 24-bit ADC per slot                   │
                    │  ±0.1g accuracy for dose verification         │
                    │                                              │
  RFID ───────────  │  MFRC522 reads cartridge RFID tag            │
                    │  Auto-identifies product in each slot         │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-C6

| Pin | Function | Notes |
|-----|----------|-------|
| IO0  | Pump 0   | MOSFET gate — slot 0 peristaltic pump |
| IO1  | Pump 1   | MOSFET gate — slot 1 |
| IO2  | Pump 2   | MOSFET gate — slot 2 |
| IO3  | Pump 3   | MOSFET gate — slot 3 |
| IO4  | Valve 0  | MOSFET gate — slot 0 anti-drip solenoid |
| IO5  | Valve 1  | slot 1 |
| IO6  | Valve 2  | slot 2 |
| IO7  | Valve 3  | slot 3 |
| IO8  | HX711-0  | Load cell 0 data |
| IO9  | HX711-0  | Load cell 0 clock |
| IO10 | HX711-1  | Load cell 1 data |
| IO11 | HX711-1  | Load cell 1 clock |
| IO12 | HX711-2  | Load cell 2 data |
| IO13 | HX711-2  | Load cell 2 clock |
| IO14 | HX711-3  | Load cell 3 data |
| IO15 | HX711-3  | Load cell 3 clock |
| IO16 | OLED SDA | I2C to SSD1306 |
| IO17 | OLED SCL | I2C to SSD1306 |
| IO18 | RFID SDA | SPI to MFRC522 |
| IO19 | RFID SCK | SPI |
| IO20 | RFID MOSI| SPI |
| IO21 | RFID MISO| SPI |
| IO22 | RFID CS  | MFRC522 chip select |
| IO23 | RFID RST | MFRC522 reset |
| IO24 | Button 0 | Manual dispense slot 0 |
| IO25 | Button 1 | Manual dispense slot 1 |
| IO26 | Button 2 | Manual dispense slot 2 |
| IO27 | Button 3 | Manual dispense slot 3 |
| IO28 | SX CS    | SX1262 SPI CS |
| IO29 | SX SCK   | SX1262 SPI SCK |
| IO30 | SX MOSI  | SX1262 SPI MOSI |
| IO31 | SX MISO  | SX1262 SPI MISO |

## Power

- 5V USB-C mains → TP4056 → 18650 2600mAh backup → 3.3V LDO (MCU)
- Pumps: 3V direct from battery via MOSFET (no LDO — pumps need full voltage)
- Solenoids: 3V normally-closed, only energized briefly during dispensing
- HX711: 3.3V, very low power (<1mA each)
- Total active: ~500mA (1 pump + valves + sensors)
- Battery backup: ~24 hours of normal operation without mains

## Pump Calibration

Each peristaltic pump has a calibrated flow rate (ml/sec) stored in flash:
- Cleanser: ~0.3 ml/sec (thicker viscosity)
- Serum: ~0.5 ml/sec
- Moisturizer: ~0.4 ml/sec
- Sunscreen: ~0.35 ml/sec (thicker)

Calibration: dispense 10ml, measure with load cell, compute actual ml/sec, store in flash. Recalibrate when product changes (RFID triggers recalibration prompt).

## Load Cell Design

Each slot has a 5kg load cell under the product cartridge:
- Product sits on load cell → weight tracked continuously
- Dispensing: measure before + after → actual amount dispensed (±0.1g)
- Empty detection: weight < cartridge tare + 5g = empty
- Low product: weight < 15% of initial fill → low-product alert
- Auto-reorder: cloud projects 7-day runway from usage rate

## Physical Design

- PCB: 120×80mm 2-layer FR4
- Enclosure: 130×90×60mm ABS countertop unit
- 4× removable product cartridges (50ml each) on top
- OLED + 4 buttons on front face
- Pumps + valves internal, tubing routes to dispensing nozzle on front
- USB-C on back
- RFID antenna under each cartridge slot
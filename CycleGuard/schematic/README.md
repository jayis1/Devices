# CycleGuard Schematics

KiCad 7+ schematic files for each hardware node:

| Node | File | SoC | Key Components |
|------|------|-----|----------------|
| Hub | `hub/hub.sch` | ESP32-S3-WROOM-1-N8R8 | SX1262, OV5640, NEO-M9N, SIM7600G, ILI9341, MAX98357A |
| Smart Helmet | `smart-helmet/smart-helmet.sch` | nRF52840 | ICM-42688-P, NAU88C22, MAX98357A, DRV2605L |
| Smart Light | `smart-light/smart-light.sch` | ESP32-C6-MINI-1 | Luxeon 1000lm, WS2812B×8, ICM-42688-P, APDS9301, PT4115 |
| Bike Sensor | `bike-sensor/bike-sensor.sch` | RP2040 + nRF52840 | A1304×2, SP37T TPMS, W25Q16JV |
| Smart Lock | `smart-lock/smart-lock.sch` | ESP32-C6-MINI-1 | SX1262, SIM7600G, CAM-M8Q, ICM-42688-P, AS5600, HX711, DRV8871 |

Open each `.sch` file in KiCad 7+ to view and edit the schematic.
Generate Gerbers for PCB fabrication via JLCPCB (4-layer FR4 recommended).
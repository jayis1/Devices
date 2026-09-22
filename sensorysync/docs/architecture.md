# SensorySync architecture

Author: jayis1

## Purpose and boundary

SensorySync helps sensory-sensitive people notice and reduce environmental overload at home, school, or work. It is a low-voltage comfort aid, not a medical device, diagnosis, restraint, or substitute for human support. It must never control mains circuits, doors, medication, or life-safety systems. Every ambient change has a physical local override.

## Nodes, power, and data flow

```text
Room Beacon xN -- Sub-GHz TDMA --+
Comfort Band ---- BLE / TDMA -----+--> Sensory Hub --> local MQTT/FastAPI --> mobile prototype
Ambient Controller -- TDMA -------+
Quiet Pod --------- TDMA ---------+
```

| Node | Function | Peak power | Safe failure |
|---|---|---:|---|
| Sensory Hub | CM4 event store and ESP32-S3 radio gateway | 5 V / 2.5 A | records stop; endpoints remain manual |
| Room Beacon | measures light, estimated sound, CO2, temperature/RH | 5 V / 450 mA | marks data stale; no inference |
| Comfort Band | wearer button, EDA trend, IMU activity | CR2477 / 18 mA burst | no alert or actuation |
| Ambient Controller | 12 V SELV LED PWM and 5 V USB fan output | 5 V / 1.5 A | outputs off; manual switch works |
| Quiet Pod | local pink-noise speaker and haptic driver | 5 V / 1.8 A | amplifier disabled; manual volume works |

The hub stores a bounded SQLite history. Nodes retain 32 records. A duplicate `(node_id, epoch, seq)` is rejected. Missing clocks are marked `time_untrusted`, never silently current. Cloud relay is opt-in; raw audio is never retained. The room microphone produces only RMS/A-weighted level estimates.

## Pin and voltage contract

| Node | Signal | Pin/net | Notes |
|---|---|---|---|
| Room Beacon ESP32-C6 | I2C SDA/SCL | GPIO6/GPIO7 | SCD41, VEML7700, SHTC3; 3.3 V, 4.7 k pull-ups |
| Room Beacon | I2S mic BCLK/WS/DOUT | GPIO2/GPIO3/GPIO4 | INMP441, 3.3 V |
| Room Beacon | SX1262 SPI | GPIO10/11/12/13; DIO1 GPIO5 | NSS/MOSI/MISO/SCK order in schematic |
| Comfort Band nRF52840 | EDA ADC / button | P0.02 / P0.13 | protected ADC; button pull-up |
| Comfort Band | IMU I2C | P0.26/P0.27 | BMI270, 3.0 V |
| Ambient Controller STM32G0B1 | LED PWM / fan enable | PA8 / PB0 | logic MOSFET gates; SELV outputs only |
| Ambient Controller | SX1262 SPI1 | PA5/PA6/PA7, PA4 NSS, PB5 DIO1 | 3.3 V radio |
| Quiet Pod RP2040 | I2S BCLK/LRCLK/DIN | GPIO10/11/12 | MAX98357A 5 V amp, 3.3 V logic |
| Quiet Pod | haptic PWM / button | GPIO15 / GPIO2 | DRV2605L enable; button local stop |

Every board needs input fuse/polyfuse, reverse-polarity protection, 100 nF local decoupling per IC, 10 uF regulator bulk capacitance, labelled test points, and antenna keep-out. Connection notes are reference designs, not fabrication-ready or certified schematics.

## Privacy, resilience, and commissioning

Use a unique per-node key provisioned outside this repository, TLS between hub and broker/API, and broker ACLs constrained to `sensorysync/v1/<node_id>/...`. No keys, recordings, identity, location, or clinical conclusions belong in this repository. On radio, broker, hub, or model failure, controllers turn off and show manual controls; they do not automate from stale values.

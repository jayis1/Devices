# WanderSync Room Sentinel — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM, dual-core 240 MHz
- Vector instructions for ADLNet CNN inference

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI interface (MOSI=GPIO7, MISO=GPIO8, SCK=GPIO9, NSS=GPIO10)
- DIO1 interrupt (GPIO11), RST (GPIO12), BUSY (GPIO13)
- PCB trace antenna (internal)

## mmWave Radar: HLK-LD2410

- 24 GHz mmWave presence sensor
- Detects human presence, motion, and range (0–6 m)
- Privacy-first: no image, no audio — just presence + motion intensity + distance
- UART interface (GPIO4 RX ← radar TX, GPIO5 TX → radar RX)
- 256600 baud, data frames with motion energy + range bins

## PIR Sensor: AM612

- Passive IR occupancy detection
- Digital output on GPIO6
- Complements mmWave for activity classification (ADLNet fuses both)

## Edge AI: TFLite-Micro

- ADLNet int8 quantized (~90 KB)
- 8-class activity classification (absent, walking, sitting, lying, eating, cooking, pacing, standing)
- Inference <80 ms per 10-second window

## Power

- USB-C 5V wall → TPS25940 eFuse → AP2112K-3.3 LDO
- MCP73871 LiPo charger (1200 mAh, 16-hour backup)
- Battery voltage on GPIO15 (ADC)
- Wall power detect on GPIO16

## Other

| Component | Interface | Pin |
|-----------|-----------|-----|
| SK6812 LED | GPIO | GPIO14 |

## Enclosure

- 3D-printed ASA, wall-mounted
- 60 × 45 × 22 mm, IP54
- Mounting: 3M VHB double-sided tape or screws

## KiCad Project

Open `room_sentinel.kicad_pro` in KiCad 7+. Schematic sheets:
1. `room_sentinel MCU.sch` — ESP32-S3 + decoupling
2. `room_sentinel Power.sch` — USB-C, MCP73871, TPS25940, LDO, LiPo 1200 mAh
3. `room_sentinel SubGHz.sch` — SX1262 + matching network + PCB antenna
4. `room_sentinel Radar.sch` — HLK-LD2410 + UART + mounting
5. `room_sentinel Peripherals.sch` — AM612 PIR, LED
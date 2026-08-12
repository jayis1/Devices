# WanderSync Voice Node — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM, dual-core 240 MHz
- Vector instructions for KeywordNet CNN inference

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI interface (MOSI=GPIO14, MISO=GPIO15, SCK=GPIO16, NSS=GPIO17)
- DIO1 interrupt (GPIO18), RST (GPIO19), BUSY (GPIO20)
- PCB trace antenna (internal)

## Microphone: INMP441

- I²S MEMS microphone, -26 dBFS sensitivity
- 16 kHz 32-bit capture for keyword detection
- I²S interface: WS=GPIO4, SCK=GPIO5, SD=GPIO6
- No audio recording — only on-device keyword detection

## Audio Amplifier: MAX98357A

- I²S Class-D amplifier for voice reminder speaker
- I²S interface: BCLK=GPIO7, LRCLK=GPIO8, DATA=GPIO9
- Drives 4Ω 3W full-range speaker

## Flash Storage: W25Q128JVSIQ

- 16 MB SPI flash for pre-recorded family voice clips
- 120 clips × 10 seconds (8 kHz 16-bit mono ≈ 160 KB per clip)
- SPI interface: CS=GPIO10, SCK=GPIO11, MOSI=GPIO12, MISO=GPIO13

## Edge AI: TFLite-Micro

- KeywordNet int8 quantized (~40 KB)
- 10-keyword detection: time, help, yes, no, where, medicine, food, water, home, stop
- 1-second audio window, <100 ms inference

## Power

- USB-C 5V wall → TPS25940 eFuse → AP2112K-3.3 LDO
- MCP73871 LiPo charger (1500 mAh, 12-hour backup)
- Battery voltage on GPIO22 (ADC)
- Wall power detect on GPIO23

## Other

| Component | Interface | Pin |
|-----------|-----------|-----|
| SK6812 LED | GPIO | GPIO21 |

## Enclosure

- 3D-printed ASA, desk/nightstand mount
- 80 × 60 × 35 mm, IP54
- Speaker grille on front, microphone opening on top

## KiCad Project

Open `voice_node.kicad_pro` in KiCad 7+. Schematic sheets:
1. `voice_node MCU.sch` — ESP32-S3 + decoupling
2. `voice_node Power.sch` — USB-C, MCP73871, TPS25940, LDO, LiPo 1500 mAh
3. `voice_node SubGHz.sch` — SX1262 + matching network + PCB antenna
4. `voice_node Audio.sch` — INMP441 I²S mic, MAX98357A amp, speaker
5. `voice_node Flash.sch` — W25Q128 SPI flash + decoupling
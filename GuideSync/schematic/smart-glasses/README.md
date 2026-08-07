# Smart Glasses — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM, dual-core 240 MHz
- BLE 5.0 (peripheral to hub)
- Vector instructions for SceneNet CNN

## Power
- LiPo 3.7V 800 mAh (6-hour autonomy)
- MCP73871 USB-C charger
- AP2112K-3.3 LDO
- Battery voltage on GPIO28 (ADC)

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| OV5640 Camera | DVP parallel | GPIO4-17 (D0-D7, VSYNC, HREF, PCLK, XCLK, SIOC, SIOD) |
| VL53L5CX ToF 8×8 | I²C | GPIO18 (SDA), GPIO19 (SCL) |
| ICM-42688 IMU | I²C | GPIO20 (SDA), GPIO21 (SCL) |
| ICS-43434 I²S Mic | I²S | GPIO22 (BCLK), GPIO23 (LRCLK), GPIO24 (DATA) |
| MAX98357A Amp | I²S | GPIO25 (BCLK), GPIO26 (LRCLK), GPIO27 (DATA) |
| SK6812 RGB LED | RMT | GPIO29 |

## PCB: 6-layer flex-rigid
- Rigid sections: MCU + camera on bridge, battery + amp in temples
- Flex sections: connect rigid sections through frame
- Bone conduction transducers mounted on temple interior

## KiCad Project
Open `smart-glasses.kicad_pro` in KiCad 7+. Schematic sheets:
1. `glasses MCU.sch` — ESP32-S3 + decoupling
2. `glasses Camera.sch` — OV5640 + FPC connector
3. `glasses ToF.sch` — VL53L5CX + I²C pullups
4. `glasses IMU.sch` — ICM-42688
5. `glasses Audio.sch` — MAX98357A + ICS-43434 + bone conduction
6. `glasses Power.sch` — MCP73871 + LDO + LiPo
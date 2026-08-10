# Room Sentinel — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM, vector instructions for FlameNet CNN

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node + relay
- SPI (MOSI=GPIO8, MISO=GPIO9, SCK=GPIO10, NSS=GPIO11)
- DIO1 (GPIO12), RST (GPIO13), BUSY (GPIO14)
- PCB trace antenna (internal)

## Sensors

| Component | Interface | Pins |
|-----------|-----------|------|
| PMSA003 smoke (photoelectric PM2.5) | I²C bus 0 | GPIO4 (SDA), GPIO5 (SCL) |
| MLX90640 thermal array (32×24 IR) | I²C bus 1 | GPIO6 (SDA), GPIO7 (SCL) |
| ZE07-CO (electrochemical CO) | UART2 | GPIO16 (RX←CO TX), GPIO17 (TX→CO RX) |
| DS18B20 temperature | 1-Wire | GPIO15 |
| AM612 PIR occupancy | GPIO input | GPIO18 |

## Outputs

| Component | Interface | Pins |
|-----------|-----------|------|
| Buzzer 85 dB | PWM (LEDC) | GPIO20 |
| Strobe LED | GPIO | GPIO21 |
| SK6812 status LED | RMT | GPIO19 |

## Power

- USB-C 5V wall → TPS25940 eFuse → AP2112K-3.3 LDO
- MCP73871 LiPo charger (1000 mAh, 14-hour backup)
- Battery voltage on GPIO22 (ADC)
- Wall power detect on GPIO23

## I²C Bus Layout

- Bus 0 (100 kHz): PMSA003 smoke sensor (0x12)
- Bus 1 (400 kHz): MLX90640 thermal array (0x33)
- Separate buses to avoid contention (MLX90640 requires 400 kHz, PMSA003 uses 100 kHz)

## Enclosure

- 3D-printed ASA, ceiling-mounted, 80 mm diameter, 35 mm thick
- IP54 with vents for smoke/CO ingress
- Vents positioned for smoke flow-through (convective)

## KiCad Project

1. `sentinel MCU.sch` — ESP32-S3 + decoupling
2. `sentinel SubGHz.sch` — SX1262 + PCB antenna
3. `sentinel Sensors.sch` — PMSA003, ZE07-CO, DS18B20, MLX90640, PIR
4. `sentinel Power.sch` — USB-C, MCP73871, TPS25940, LDO, LiPo
5. `sentinel Outputs.sch` — Buzzer, strobe, status LED
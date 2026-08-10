# Stove Guard — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI (MOSI=GPIO8, MISO=GPIO9, SCK=GPIO10, NSS=GPIO11)
- DIO1 (GPIO12), RST (GPIO13), BUSY (GPIO14)

## Sensors

| Component | Interface | Pins |
|-----------|-----------|------|
| MLX90640 thermal array (32×24) | I²C bus 1 | GPIO6 (SDA), GPIO7 (SCL) |
| AS5600 knob encoder ×4 | I²C bus 0 via TCA9548A mux | GPIO4 (SDA), GPIO5 (SCL) |
| Reed switch (valve feedback) | GPIO input | GPIO17 |

## TCA9548A I²C Mux

- 8-channel I²C multiplexer at address 0x70
- Channels 0-3: AS5600 magnetic encoders (burner knobs 1-4)
- AS5600 address: 0x36 (all share same address → mux required)

## Gas Valve Control (L298N H-Bridge)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO15 | Valve open drive | L298N IN1 (energize to open) |
| GPIO16 | Valve close drive | L298N IN2 (energize to close, redundant with spring return) |
| GPIO17 | Valve position | Reed switch (0 = fully closed) |

- Motorized ball valve: 12V DC latching, 1/2" NPT, fails CLOSED (spring return)
- L298N powered from MT3608 boost converter (5V → 12V)

## Power

- USB-C 5V wall → TPS25940 eFuse → AP2112K-3.3 LDO
- MT3608 boost to 12V for gas valve motor
- MCP73871 LiPo charger (1200 mAh, 10-hour backup)
- Battery voltage on GPIO20 (ADC)

## Thermal Array Placement

- MLX90640 mounted under range hood, looking down at stovetop
- FOV covers all 4 burners (32×24 zones over ~60×45 cm stovetop)
- Heat-resistant enclosure (ASA, rated to 85°C ambient)

## KiCad Project

1. `stove MCU.sch` — ESP32-S3 + decoupling
2. `stove SubGHz.sch` — SX1262 + PCB antenna
3. `stove Thermal.sch` — MLX90640 + I²C pull-ups
4. `stove Knobs.sch` — TCA9548A + 4× AS5600
5. `stove Valve.sch` — L298N H-bridge + motorized ball valve + reed switch
6. `stove Power.sch` — USB-C, MCP73871, MT3608 boost, LiPo
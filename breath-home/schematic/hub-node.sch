EESchema-LIBRARY Version 2.4
# BreathHome Hub Node Schematic
# nRF5340 + ESP32-C6 + SX1262 + Sensors + Display + Audio

# Power Section
# USB-C 5V → MCP73831 → Lipo 3000mAh → AP2112-3.3V → nRF5340 + ESP32-C6
#                                      → AP6212-1.8V → SX1262
#                                      → 5V direct → MAX98357A + TFT backlight

# nRF5340 Connections:
# P0.00/P0.01 → ESP32-C6 UART1 TX/RX (inter-MCU link)
# P0.02/P0.03 → I2C0: SCD41 (0x62) + BME688 (0x76)
# P0.04/P0.05 → I2C1: SGP41 (0x59) + SFA30 (0x6D)
# P0.06-P0.10 → SPI0: W25Q128 Flash (CS0) + SD Card (CS1)
# P0.11-P0.17 → SPI1: ILI9341 TFT Display
# P0.18-P0.21 → SX1262 SPI + control signals
# P0.22-P0.25 → I2S audio: BCLK, WS, DOUT (speaker), DIN (mic)
# P0.26 → Emergency button (active low, internal pull-up)
# P0.27-P0.29 → RGB LED
# P0.30/P0.31 → Zone 1/2 LEDs
# P1.00/P1.01 → Zone 3/4 LEDs

# ESP32-C6 Connections:
# GPIO4/GPIO5 → nRF5340 UART0 TX/RX
# GPIO12/GPIO13 → USB D+/D-
# GPIO0/GPIO1 → I2C expansion port

# SX1262 Connections:
# SPI: SCK, MISO, MOSI → nRF5340 SPI bus
# NSS → P0.21
# BUSY → P0.18
# IRQ → P0.19
# NRST → P0.20
# ANT → SMA connector → 868MHz antenna

# Audio:
# MAX98357A: I2S from nRF5340 → 3W speaker
# SPH0645LM4H: I2S to nRF5340 ← MEMS microphone

Component List:
1x nRF5340-QKAA-AB0 (U1) - Main MCU
1x ESP32-C6-MINI-1 (U2) - WiFi bridge
1x SX1262IMLTRT (U3) - Sub-GHz radio
1x SPS30 (U4) - Particulate sensor
1x SCD41 (U5) - CO2 sensor
1x BME688 (U6) - Environment sensor
1x SGP41 (U7) - VOC/NOx sensor
1x SFA30 (U8) - Formaldehyde sensor
1x ILI9341 3.2" TFT (U9) - Display
1x MAX98357A (U10) - Speaker amp
1x SPH0645LM4H (U11) - MEMS mic
1x W25Q128 (U12) - 16MB flash
3x AP2112-3.3 (U13) - 3.3V LDO
1x AP6212-1.8 (U14) - 1.8V LDO
1x MCP73831 (U15) - Lipo charger
1x Lipo 3000mAh (BAT1)
1x USB-C connector (CON1)
1x SMA antenna connector (ANT1)
1x 30mm tactile button (SW1)
1x RGB LED 5050 (LED1)
4x LED 0805 (LED2-5)
1x Piezo buzzer (BUZ1)
1x 3W 40mm speaker (SPK1)
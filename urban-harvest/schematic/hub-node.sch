EESchema-LIBNEW Version 2.4
# UrbanHarvest Hub Node Schematic
# nRF5340 + ESP32-C6 + SX1262 + TFT + Audio + Sensors
#
# BLOCK DIAGRAM:
#
#  USB-C 5V ──┬── MCP73831 ── Lipo 3000mAh ──┬── AP2112-3.3V ── nRF5340
#             │                               ├── AP2112-3.3V ── ESP32-C6
#             │                               ├── AP6212-1.8V ── SX1262
#             │                               └── 5V ── TFT backlight + MAX98357A
#             │
#  nRF5340 ─── UART0 ── ESP32-C6 (inter-MCU link, 115200 baud)
#  nRF5340 ─── I2C0 ── BME688 + TSL25911
#  nRF5340 ─── I2C1 ── (expansion)
#  nRF5340 ─── SPI0 ── W25Q128 Flash + MicroSD
#  nRF5340 ─── SPI1 ── ILI9341 TFT display
#  nRF5340 ─── SPI2 ── SX1262 (Sub-GHz radio)
#  nRF5340 ─── I2S  ── MAX98357A (speaker) + SPH0645LM4H (mic)
#  nRF5340 ─── GPIO ── RGB LED, zone LEDs, buzzer
#
# Component list:
#   U1:  nRF5340-QIAA (aQFN94, 7x7mm)
#   U2:  ESP32-C6-MINI-1 (module, 18x18mm)
#   U3:  SX1262IMLTRT (QFN24, 4x4mm)
#   U4:  ILI9341 3.2" TFT (connector FPC 40pin)
#   U5:  MAX98357A (QFN16, 3x3mm)
#   U6:  SPH0645LM4H (MEMS mic, bottom-port)
#   U7:  BME688 (LGA14, 3x3mm)
#   U8:  TSL25911FN (DFN6, 2x2mm)
#   U9:  W25Q128JVEIQ (SOIC8)
#   U10: MCP73831 (SOT23-5)
#   U11: AP2112-3.3 (SOT23-5)
#   U12: AP6212-1.8 (SOT23-5)
#   U13: MicroSD slot (push-push)
#   X1:  3W 8Ω speaker (40mm)
#   X2:  Piezo buzzer (3V, 85dB)
#   X3:  USB-C 16pin receptacle
#   BT1: Lipo 3000mAh (103040)
#   D1-D5: RGB LEDs (3528)
#
# Crystal:
#   Y1: 32MHz for nRF5340 (ABM8-32.000MHZ-20-B2X-T)
#   Y2: 40MHz for ESP32-C6 (built into module)
#   Y3: 32.768kHz RTC crystal (FC-135)
#
# Antenna:
#   ANT1: 868MHz chip antenna (Johanson 245AT18A300)
#   ANT2: 2.4GHz PCB trace (nRF5340 onboard)
#   ANT3: 2.4GHz PCB trace (ESP32-C6 onboard)
#
# Decoupling: 100nF per VDD pin + 10µF bulk per rail
# I2C pullups: 4.7kΩ on each bus
# SPI lines: 33Ω series termination resistors
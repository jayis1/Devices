EESchema-LIBRARY Version 2.4
# BreathHome Wearable Breath Tag Schematic
# nRF52832 + SGP30 + SHT40 + LIS2DH12 + Lipo + USB-C

# Power Section:
# Lipo 3.7V 120mAh → TLV73333P (3.3V LDO) → nRF52832 + SGP30 + SHT40 + LIS2DH12
# USB-C 5V → MCP73831 → Lipo charging
# All sensors run directly on 3.3V (no level shifters needed)

# nRF52832 Connections:
# P0.02/P0.03 → I2C0: SGP30 (0x58) - eCO2 + TVOC
# P0.04/P0.05 → I2C1: SHT40 (0x44) - temperature + humidity
# P0.06 → SGP30_RESET (active low)
# P0.07/P0.08 → I2C2: LIS2DH12 (0x18) - accelerometer
# P0.09 → LIS2DH_INT1 - accelerometer interrupt 1
# P0.10 → LIS2DH_INT2 - accelerometer interrupt 2
# P0.11 → BTN - symptom button (active low, internal pull-up)
# P0.12 → VIBRATE - ERM motor driver (NPN open collector)
# P0.13 → LED_DATA - WS2812B mini RGB LED data
# P0.14 → CHG_STATUS - MCP73831 charge status
# P0.15 → VBAT_SENSE - Lipo voltage ADC (through 1:2 voltage divider: 100k/100k)
# P0.18 → SWDIO - debug/programming
# P0.19 → SWCLK - debug/programming

# SGP30 eCO2 + TVOC Sensor:
# I2C address 0x58
# Ultra-low-power mode: ~40µA average
# Warm-up: 20 seconds for stable readings
# Humidity compensation input from SHT40

# SHT40 Temperature + Humidity:
# I2C address 0x44
# Accuracy: ±0.1°C, ±1.8% RH
# Power: 0.4µA standby, 0.9mA measuring
# Measurement time: ~10ms

# LIS2DH12 Accelerometer:
# I2C address 0x18 (or 0x19 with SDO high)
# 12.5 Hz ODR for always-on activity detection
# ±2g range, high-resolution mode
// Click detection for button-less UI
// Activity/inactivity interrupts on INT1/INT2

# Vibration Motor Driver:
# nRF52832 GPIO (PWM) → 100Ω → NPN base (MMBT3904) → ERM motor
# Flyback diode across motor
# PWM frequency: 200Hz for smooth vibration

# WS2812B Mini RGB LED:
# Single LED, 2mm×2mm package
// Data driven by nRF52832 GPIO bit-bang
// Color mapping: Green=AQI≤50, Yellow=50-100, Orange=100-150, Red=150-200, Purple=>200

# Battery Charging:
# MCP73831: 500mA charge current for 120mAh Lipo
# CHG status → nRF52832 P0.14
# PROG resistor: 2kΩ → 500mA charge rate

Component List:
1x nRF52832-QFAA (U1) - BLE MCU
1x SGP30 (U2) - eCO2 + TVOC
1x SHT40-AD1B (U3) - Temperature + humidity
1x LIS2DH12 (U4) - Accelerometer
1x TLV73333P (U5) - 3.3V LDO
1x MCP73831 (U6) - Lipo charger
1x Lipo 120mAh (BAT1)
1x ERM motor 4mm (MOT1)
1x WS2812B mini (LED1)
1x USB-C connector (CON1)
1x 8mm tactile button (SW1)
1x MMBT3904 (Q1) - Motor driver NPN
1x 2N7002 (Q2) - Level shifter (if needed)
EESchema-LIBRARY Version 2.4
# BreathHome Room Sensor Node Schematic
# STM32WB55CG + SPS30 + SCD41 + SGP41 + SFA30 + BME688 + TSL25911 + SX1261

# Power Section:
# USB-C 5V → AP2112-3.3V → STM32WB55 + all sensors + SX1261
# 3x AA (4.5V) → AP2112-3.3V → battery backup

# STM32WB55CG Connections:
# PA0/PA1 → I2C1: SPS30 (0x69) - particulate sensor
# PA2/PA3 → I2C2: SCD41 (0x62) - CO2 sensor
# PA4/PA5 → I2C3: SGP41 (0x59) + SFA30 (0x6D) - VOC/NOx + HCHO
# PA6/PA7 → I2C4: BME688 (0x76) - environment
# PB6/PB7 → I2C5: SX1261 Sub-GHz radio
# PB10/PB11 → UART1: SPS30 (alternative UART mode)
# PB12 → SPS30_RESET
# PB13 → SCD41_RESET
# PB14 → SX1261_BUSY
# PB15 → SX1261_IRQ
# PC0 → SX1261_NRST
# PC1 → SX1261_NSS
# PC2/PC3/PC4 → SPI1: SX1261 (SCK/MISO/MOSI)
# PC5 → TSL25911_INT - light sensor interrupt
# PC6/PC7 → UART2: RD200M radon sensor (basement nodes only)
# PA8/PA9/PA10 → RGB LED
# PA11 → Setup/pairing button
# PB0 → VBAT_SENSE ADC
# PA13/PA14 → SWD debug

# SPS30 Particulate Sensor:
# I2C address 0x69 or UART at 115200 baud
# Fan runs continuously at low power (8mA)
# Measurement: PM1.0, PM2.5, PM4.0, PM10 every 1s

# SCD41 CO2 Sensor:
# I2C address 0x62
# Photoacoustic NDIR principle
# Range: 400-5000ppm, accuracy ±40ppm + 5%
# Measurement interval: 30s (can be lowered to 5s)

# SGP41 VOC/NOx Sensor:
# I2C address 0x59
# Raw signals SRAW_VOC and SRAW_NOX
# Requires 64 readings for conditioning
# Humidity compensation from SHT40 or SCD41

# SFA30 Formaldehyde Sensor:
# I2C address 0x6D
# Range: 0-1ppm HCHO
# Resolution: 0.01ppm

# BME688 Environment Sensor:
# I2C address 0x76
# Temperature: ±0.5°C, Humidity: ±3%RH, Pressure: ±1hPa
# Gas heater: 320°C, 150ms measurement

# SX1261 Sub-GHz Radio:
# SPI + control signals
# 868MHz LoRa modulation
# TX power: +14dBm max
# SF7 (normal), SF9 (long range alert)

Component List:
1x STM32WB55CGU6 (U1) - Main MCU + radio
1x SPS30 (U2) - Particulate sensor
1x SCD41 (U3) - CO2 sensor
1x SGP41 (U4) - VOC/NOx sensor
1x SFA30 (U5) - Formaldehyde sensor
1x BME688 (U6) - Environment sensor
1x TSL25911 (U7) - Light sensor
1x SX1261IMLTRT (U8) - Sub-GHz radio
0-1x RD200M (U9) - Radon sensor (basement only)
2x AP2112-3.3 (U10) - 3.3V LDO
1x USB-C connector (CON1)
1x 3x AA battery holder (BAT1)
1x 868MHz chip antenna (ANT1)
1x RGB LED 5050 (LED1)
1x 6mm tactile button (SW1)
EESchema-LIBNEW Version 2.4
# UrbanHarvest Grow Pod Controller Schematic
# ESP32-S3 + OV2640 + 4ch LED driver + pumps + climate
#
# BLOCK DIAGRAM:
#
#  24V DC ──┬── LM2596-5.0 ── ESP32-S3 + sensors + logic
#           ├── LM2596-12.0 ── water pump + fans + stepper motors
#           ├── AL8860 #1 ── Deep Red LED strip (660nm)
#           ├── AL8860 #2 ── Royal Blue LED strip (450nm)
#           ├── AL8860 #3 ── Warm White LED strip (3000K)
#           └── AL8860 #4 ── Far Red LED strip (730nm)
#
#  USB-C 5V ── backup power (MCU keeps running if 24V fails)
#
#  ESP32-S3 ─── CAM_IF ── OV2640 (8-bit parallel + SCCB on I2C)
#  ESP32-S3 ─── I2C  ── BME688 + TSL25911 + OV2640 SCCB
#  ESP32-S3 ─── ONEWIRE ── DS18B20 (nutrient solution temp)
#  ESP32-S3 ─── GPIO_INT ── YF-S201 (water flow, pulse counter)
#  ESP32-S3 ─── PWM ── AL8860 #1-4 (LED dimming, 4 channels)
#  ESP32-S3 ─── MOSFET ── IRLZ44N (water pump PWM)
#  ESP32-S3 ─── STEP/DIR ── ULN2003 #1 → 28BYJ-48 (nutrient A)
#  ESP32-S3 ─── STEP/DIR ── ULN2003 #2 → 28BYJ-48 (nutrient B)
#  ESP32-S3 ─── STEP/DIR ── DRV8833 → 0622A (pH doser)
#  ESP32-S3 ─── PWM ── Fan speed control (MOSFET)
#  ESP32-S3 ─── GPIO ── SSR-25DA (heater)
#  ESP32-S3 ─── GPIO ── Ultrasonic humidifier MOSFET
#  ESP32-S3 ─── GPIO ── 4× OMRON G5LE-14 relays
#  ESP32-S3 ─── SPI2 ── SX1261 (Sub-GHz radio)
#
# Component list:
#   U1:  ESP32-S3-WROOM-1-N8R8 (module)
#   U2:  OV2640-FS1019 (camera module, 24pin FPC)
#   U3:  SX1261IMLTRT (QFN24, 4x4mm)
#   U4-U7: AL8860MP-13 (SOT89-5, LED drivers)
#   U8:  ULN2003A #1 (DIP16 or SOIC16)
#   U9:  ULN2003A #2
#   U10: DRV8833 (HTSSOP16)
#   U11: IRLZ44N (TO-220, water pump MOSFET)
#   U12: BME688 (LGA14)
#   U13: TSL25911FN (DFN6)
#   U14: DS18B20 (waterproof probe, 1m cable)
#   U15: LM2596-5.0 (TO-263-5)
#   U16: LM2596-12.0 (TO-263-5)
#   U17: SSR-25DA (panel mount)
#   K1-K4: OMRON G5LE-14 (5V relay)
#   M1:  12V DC fan (80mm, 3-wire PWM)
#   M2:  Ultrasonic mist maker (24V, 5W)
#   M3:  28BYJ-48 stepper #1 (nutrient A peristaltic pump)
#   M4:  28BYJ-48 stepper #2 (nutrient B peristaltic pump)
#   M5:  0622A micro peristaltic pump (pH dosing)
#   P1:  12V DC submersible water pump
#   F1:  YF-S201 water flow sensor
#   J1:  24V DC barrel jack (5.5×2.1mm)
#   J2:  USB-C receptacle (backup power)
#   J3:  LED strip connector #1 (4-pin, red)
#   J4:  LED strip connector #2 (4-pin, blue)
#   J5:  LED strip connector #3 (4-pin, white)
#   J6:  LED strip connector #4 (4-pin, far red)
#   J7:  Drip line output connector (quick-connect)
#   J8:  Nutrient A input tube connector
#   J9:  Nutrient B input tube connector
#   J10: pH solution input tube connector
#   ANT1: 868MHz chip antenna (Johanson 245AT18A300)
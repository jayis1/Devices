EESchema-LIBRARY Version 2.4
# BreathHome HVAC Controller Node Schematic
# ESP32-S3 + CC2652R7 (Zigbee) + SX1261 (Sub-GHz) + Relays + 433MHz TX

# Power Section:
# 24VAC from furnace transformer → HLK-PM01 (5V/3W AC-DC) → AP2112-3.3V → ESP32-S3 + CC2652R7 + SX1261
# 5V USB-C → backup power (auto-switches on 24VAC loss)

# ESP32-S3 Connections:
# GPIO1/GPIO2 → UART0: CC2652R7 TX/RX (Zigbee coordinator)
# GPIO3 → CC2652_RESET
# GPIO4 → CC2652_BOOT
# GPIO5/6/7 → SPI2: SX1261 (SCK/MISO/MOSI)
# GPIO8 → SX1261_NSS
# GPIO9 → SX1261_BUSY
# GPIO10 → SX1261_IRQ
# GPIO11 → SX1261_NRST
# GPIO12/GPIO13 → I2C: BMP390 (0x77) - duct pressure
# GPIO14 → ONE_WIRE: DS18B20 - supply air temperature
# GPIO15 → ADC: SCT013-030 current sensor
# GPIO16/GPIO17 → I2C2: BME688 (0x76) - ambient environment
# GPIO18 → RELAY_1: Furnace fan override (OMRON G5LE-14)
# GPIO19 → RELAY_2: Bathroom exhaust (OMRON G5LE-14)
# GPIO20 → RELAY_3: Range hood (OMRON G5LE-14)
# GPIO21 → RELAY_4: Whole-house fan (OMRON G5LE-14)
# GPIO22 → 433M_TX: FS1000A data
# GPIO23 → BUZZER: Piezo status buzzer
# GPIO25/26/27 → RGB LED
# GPIO33 → Setup button
# GPIO34/35 → USB D+/D-

# Relay Driver Circuit (per relay):
# ESP32-S3 GPIO → 1kΩ → NPN base (2N2222) → Relay coil → flyback diode
# Relay contacts: dry SPDT (COM, NO, NC) to screw terminals

# CC2652R7 Zigbee Coordinator:
# UART0 to ESP32-S3 for command/response
# Zigbee 3.0 coordinator firmware (Z-Stack)
# Controls: smart vents, air purifiers, thermostats via Zigbee
# Antenna: 2.4GHz PCB trace + SMA for external

# SCT013-030 Current Sensor:
# Non-invasive clamp on furnace blower power wire
# Output: 0-1V AC proportional to 0-30A
# Burden resistor on PCB: 33Ω
# ESP32-S3 ADC reads rectified peak

# DS18B20 Temperature Probe:
# Waterproof probe inserted into supply air duct
# 1-Wire interface with 4.7kΩ pullup to 3.3V

Component List:
1x ESP32-S3-WROOM-1-N8R8 (U1) - Main MCU
1x CC2652R7 (U2) - Zigbee coordinator
1x SX1261IMLTRT (U3) - Sub-GHz radio
1x BMP390 (U4) - Duct pressure
1x BME688 (U5) - Ambient environment
1x HLK-PM01 (U6) - 24VAC to 5V AC-DC
4x OMRON G5LE-14 5V (RL1-4) - Relays
1x FS1000A (TX1) - 433MHz transmitter
1x SCT013-030 (CT1) - 30A current sensor
1x DS18B20 waterproof (TH1) - Supply air temp
2x AP2112-3.3 (U7) - 3.3V LDO
4x NPN 2N2222A - Relay drivers
4x 1N4007 - Flyback diodes
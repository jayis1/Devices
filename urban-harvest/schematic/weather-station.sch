EESchema-LIBNEW Version 2.4
# UrbanHarvest Weather Station Node Schematic
# RP2040 + SX1262 + wind + rain + solar + environmental sensors
#
# BLOCK DIAGRAM:
#
#  5W 6V Solar ── BQ25570 (MPPT) ── Lipo 2000mAh ── AP2112-3.3V ── RP2040 + sensors
#                                                ── 3.6V ── SX1262
#
#  RP2040
#    ├── I2C0 (GPIO0/1) ── SHT45 (temp/humidity) + BMP390 (pressure)
#    ├── I2C1 (GPIO2/3) ── VEML6075 (UV) + TSL25911 (light)
#    ├── SPI0 (GPIO4/5/6) ── SX1262 (Sub-GHz radio)
#    ├── GPIO7 ── SX1262_NSS
#    ├── GPIO8 ── SX1262_BUSY
#    ├── GPIO9 ── SX1262_IRQ
#    ├── GPIO10 ── SX1262_NRST
#    ├── GPIO11 ── Anemometer (reed switch, GPIO interrupt, counter)
#    ├── GPIO12 ── Wind vane (ADC, potentiometer 0-360°)
#    ├── GPIO13 ── Rain gauge (reed switch, GPIO interrupt, counter)
#    ├── GPIO14 ── Solar panel voltage ADC (voltage divider)
#    ├── GPIO15 ── Battery voltage ADC (voltage divider)
#    ├── GPIO16 ── MCP73871 charge status input
#    ├── GPIO17 ── BQ25570 enable (active high)
#    ├── GPIO18/19/20 ── R/G/B status LEDs
#    ├── GPIO21 ── Pairing button (active low)
#    └── UART0 (GPIO24/25) ── Debug console
#
# Anemometer interface:
#   Reed switch → 10kΩ pullup → GPIO11 (interrupt on falling edge)
#   Counter accumulates rotations, wind speed = rotations × calibration factor
#   Typical: 1 rotation/second = 2.4 km/h (Davis Instruments formula)
#   Debounce: 15ms hardware (RC filter) + 10ms software
#
# Wind vane interface:
#   8-position resistive potentiometer (10kΩ range)
#   Voltage divider with 10kΩ reference resistor → ADC
#   8 directions: N(0°), NE(45°), E(90°), SE(135°), S(180°), SW(225°), W(270°), NW(315°)
#   ADC lookup table maps voltage → direction
#
# Rain gauge interface:
#   Tipping bucket reed switch → 10kΩ pullup → GPIO13 (interrupt on falling edge)
#   Each tip = 0.2794mm rainfall (0.01 inches)
#   Accumulate tips over 60-second interval, report as mm/hr
#
# Solar power management:
#   BQ25570: MPPT charge from 5W panel (6V open circuit, 0.83A short circuit)
#   Harvested voltage: 3.6V (battery float)
#   Panel voltage sense: resistor divider (100kΩ/33kΩ) → ADC, V_panel = ADC × (133/33) × 3.3
#   Battery voltage sense: resistor divider (100kΩ/100kΩ) → ADC, V_bat = ADC × 2 × 3.3
#   Battery SOC estimation: 3.0V = 0%, 3.7V = 50%, 4.2V = 100% (Lipo discharge curve)
#
# Component list:
#   U1:  RP2040 (QFN56, 7x7mm)
#   U2:  W25Q16JVSIQ (SOIC8, 2MB flash for RP2040)
#   U3:  SX1262IMLTRT (QFN24, 4x4mm)
#   U4:  SHT45-AD1B (DFN8, 2.5×2.5mm)
#   U5:  BMP390 (LGA14, 2x2mm)
#   U6:  VEML6075 (OPLGA4, 2.4x2x1mm)
#   U7:  TSL25911FN (DFN6)
#   U8:  BQ25570RGRR (QFN20, 4x4mm)
#   U9:  MCP73871 (QFN20, 4x4mm)
#   U10: AP2112-3.3 (SOT23-5)
#   Y1:  12MHz crystal (ABM8-12.000MHZ, RP2040 XIN)
#   J1:  Anemometer connector (RJ11 or screw terminal)
#   J2:  Wind vane connector (RJ11 or screw terminal)
#   J3:  Rain gauge connector (RJ11 or screw terminal)
#   J4:  Solar panel connector (XT30 or screw terminal)
#   J5:  Battery connector (JST-PH 2-pin)
#   ANT1: 868MHz chip antenna (Johanson 245AT18A300)
#   D1-D3: 0805 LEDs (R/G/B)
#   SW1: 6mm tactile button
#   F1:  Polyfuse 500mA
#
# Enclosure: IP65 Stevenson screen, ASA plastic, UV-stable
#   Louvered design for airflow while shielding from direct rain/sun
#   Internal PCB mounted vertically for convective cooling
#   Sensor openings: anemometer shaft (top), vane shaft (top), rain funnel (side)
#   Solar panel mounted on top face (tilted 30° from horizontal for optimal collection)
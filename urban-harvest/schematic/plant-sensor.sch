EESchema-LIBNEW Version 2.4
# UrbanHarvest Plant Sensor Node Schematic
# STM32WL55CC + capacitive soil moisture + EC + temp + PAR + leaf wetness
#
# BLOCK DIAGRAM:
#
#  3× AA (4.5V) ── HT7333-3.3V ── STM32WL55CC + I2C sensors
#                 ── 4.5V direct ── Soil moisture + EC drive voltage
#
#  STM32WL55CC (integrated Sub-GHz, 868MHz)
#    ├── PA0 (ADC) ── Capacitive soil moisture sensor (analog, 0-3.3V)
#    ├── PA1 (GPIO) ── EC excitation (AC square wave, 1kHz)
#    ├── PA2 (ADC) ── EC measurement (differential +)
#    ├── PA3 (ADC) ── EC reference (differential -)
#    ├── PB10 (OneWire) ── DS18B20 soil temperature
#    ├── PA4/PA5 (I2C1) ── TSL25911 PAR light sensor
#    ├── PB11 (ADC) ── Leaf wetness sensor (capacitive, analog)
#    ├── PB12 (GPIO) ── WS2812B mini RGB LED
#    ├── PB13 (GPIO) ── Pairing button (active low)
#    ├── PA8/9/10 (GPIO) ── R/G/B status LEDs (basic)
#    ├── PC0 (ADC) ── Battery voltage sense (divider)
#    └── RF_OUT ── 868MHz whip antenna (SMA)
#
# Soil EC measurement circuit:
#   PA1 drives AC square wave (1kHz, rail-to-rail) through EC electrodes
#   PA2/PA3 measure differential voltage across electrodes
#   4-wire measurement: separate excitation and sense electrodes
#   Calibration: known KCl solutions (1.413 mS/cm, 2.767 mS/cm)
#
# Capacitive soil moisture:
#   Standard capacitive v1.2 sensor (analog output, 0-3.3V)
#   Frequency-based measurement: oscillator frequency changes with dielectric
#   Calibration: dry air = ~3.2V (0%), water = ~1.5V (100%)
#   Corrosion-resistant: gold-plated PCB traces, no exposed metal
#
# Leaf wetness sensor:
#   Custom PCB: interdigitated copper traces, gold ENIG finish
#   Capacitance changes with water droplets on surface
#   Placed horizontally near plant canopy to mimic leaf surface
#   Calibration: dry = 0%, wet (sprayed) = 100%
#
# Component list:
#   U1:  STM32WL55CC (UFQFPN48, 7x7mm)
#   U2:  HT7333-3.3 (SOT89-3, LDO)
#   U3:  TSL25911FN (DFN6)
#   U4:  Capacitive soil moisture v1.2 (onboard or module)
#   U5:  DS18B20 (waterproof probe, 30cm cable, OneWire)
#   U6:  Custom leaf wetness PCB (interdigitated, 20x20mm)
#   D1:  WS2812B-mini (3.5x3.5mm)
#   D2-D4: 0805 LEDs (R/G/B, basic indicators)
#   SW1: 12mm tactile button (waterproof boot)
#   J1:  Battery holder (3× AA, Keystone 2479)
#   J2:  SMA connector (868MHz antenna)
#   ANT1: 50mm whip antenna (868MHz)
#   Y1: 32MHz (ABM8-32.000MHZ, for STM32WL HSE)
#   Y2: 32.768kHz (FC-135, for LSE/RTC)
#
# Power protection:
#   D5: TVS diode (PRTR5V0U2X, ESD on all exposed pins)
#   F1: Polyfuse 250mA (resettable, short-circuit protection)
#   C1-C4: 100nF decoupling per VDD
#   C5: 10µF bulk (input)
#   C6: 4.7µF bulk (output of LDO)
#
# Form factor: 35×120×15mm PCB, IP68 enclosure
#   Top 30mm: electronics + antenna + LED + button
#   Bottom 90mm: soil probe (moisture + EC + temp sensors)
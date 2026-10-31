# Architecture and pin/net schedules

All assignments are proposed for Raspberry Pi Pico W / RP2040 GPIO; verify board revision and module footprint before layout. Use 3.3 V logic. Pico W GPIO numbers below are MCU GPIOs.

## Inlet node (each)
- U1 Pico W; U2 VL53L0X breakout on I²C0 SDA=GPIO4, SCL=GPIO5; address 0x29.
- U3 SX1262 module on SPI0 SCK=GPIO18, MOSI=GPIO19, MISO=GPIO16, NSS=GPIO17; BUSY=GPIO20, DIO1=GPIO21, RESET=GPIO22. Regional module and antenna are mandatory.
- GPIO10 = sensor shutdown (optional); GPIO11 = status LED via 1 kΩ; GPIO12 = battery divider ADC (100 kΩ upper / 100 kΩ lower, ADC sees half supply; add high-value divider for low quiescent drain in final design).
- Supply: protected 1-cell Li-ion → 3.3 V buck/boost sized for radio peak current. Use a fused/protected cell assembly, reverse-polarity protection, local decoupling, sealed non-metallic enclosure and condensation management. No charging design specified.
- Runtime is not measured. An illustrative 2500 mAh cell at 2 mA average load would be 2500/2 = 1250 h (about 52 days), before regulator losses, cold-temperature derating, self-discharge and peak-current effects. This is not a battery-life claim; measure the final board and report low-battery/stale status.
- Range measurement: sensor faces water through a protected stilling tube, with a mechanically fixed reference distance; report raw millimeters and validity. Do not interpret a single reading as blockage.

## Rain/context node
- U1 Pico W; tipping bucket dry contact to GPIO10 with pull-up and debounce; BME280 I²C0 SDA=GPIO4/SCL=GPIO5, address 0x76 (SDO strap low; 0x77 alternative); SX1262 SPI assignments match inlet node.
- USB 5 V during prototype. Solar/battery power is not designed or claimed.

## Gateway
- Pico W + SX1262 same SPI/handshake pins; Wi-Fi uplink, GPIO11 status LED. USB 5 V. No mains or relay output.

## Wiring review notes
The SPI and I²C buses are distinct, GPIO assignments do not overlap, and RP2040 ADC GPIO26–29 are used only for analog voltage. Radio and sensor modules must tolerate 3.3 V logic. This document is a pin/net schedule, not a schematic or PCB; no electrical-rule check or physical validation has been performed. Keep antenna clear of metal and away from water. Installation requires public-works authorization and safe traffic control.

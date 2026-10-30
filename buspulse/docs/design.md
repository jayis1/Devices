# BusPulse design and bring-up

Author: jayis1. Prototype reference assignments; verify against exact board revision and component datasheets before wiring.

| Node | MCU/radio | Role | Supply |
|---|---|---|---|
| Gateway | ESP32-S3-WROOM-1 dev board, SX1276 915 MHz module | local aggregation and Wi-Fi uplink | regulated USB 5 V |
| Display Observer | ESP32-C3-MINI-1 dev board, SX1276 module | isolated display-supply proxy + ambient light | protected vendor-approved solar USB supply |
| Power/Weather | STM32L072CZ, SX1276 module, TMP117 | sentinel supply voltage and enclosure temperature | protected vendor-approved solar USB supply |

915 MHz LoRa point-to-point; gateway required, no mesh claim. Confirm local RF rules and module carrier compatibility. PHY CRC plus application CRC detect accidental corruption, not forgery. No actuator commands.

## Pin assignments

Gateway ESP32-S3: SPI SCK GPIO12, MOSI 11, MISO 13, CS 10, RESET 9, DIO0 8; UART0 console; USB 5 V input.

Display Observer ESP32-C3: SPI SCK 4, MOSI 6, MISO 5, CS 7, RESET 3, DIO0 2; ADC GPIO0 through a resistor divider for a protected, isolated display-supply monitor (ADC maximum 3.0 V); ADC GPIO1 photodiode divider. Never connect unknown external rails directly to GPIO.

Power/Weather STM32L072CZ: SPI1 SCK PA5, MISO PA6, MOSI PA7; CS PA4, RESET PB0, DIO0 PB1; I2C1 PB8/PB9 to TMP117 at 3.3 V; ADC PA0 via rated divider; SWD PA13/PA14. Provide 3.3 V regulator transient margin, local decoupling and I2C pull-ups per datasheets.

## Power assumptions and commissioning

No runtime estimate: panel, battery, regulator, RF duty cycle, climate and enclosure are site-dependent and unmeasured. Use protected commercial power assemblies with overcurrent, overcharge, undervoltage and temperature protection; do not connect raw panels. Bench-power with current limit, confirm ADC divider voltage using a meter, test sensor-open/low-voltage/missing-packet states, then obtain site-owner approval for mounting. No KiCad capture, ERC or target build is claimed. Exact carrier pinouts must be checked before wiring.

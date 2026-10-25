# Hardware interface notes

Connection-level pin map only; validate against exact module revisions and board layouts before fabrication.

| Node | Signal | MCU pin | Interface / notes |
|---|---|---|---|
| Fixture ESP32-C3-MINI-1 | flow pulse | GPIO4 | 3.3V-safe conditioned open-collector input, pull-up 10k to 3V3 |
| Fixture | leak switch | GPIO5 | switch to GND, internal pull-up, debounce |
| Fixture | service button | GPIO9 | verify module boot-strapping implications before PCB; use external programming access |
| Fixture | I2C spare | GPIO6/7 | reserved, 3V3 only |
| Gateway RP2040 | ESP32-C6 UART | GPIO0/1 | 3.3V UART; cross-connect TX/RX |
| Gateway | service button | GPIO14 | active low |
| Gateway | status LED | GPIO15 | series 1k resistor |
| Path-check nRF52840 | LIS2DH12 SDA/SCL | P0.26/P0.27 | 3.3V I2C, address 0x18; 4.7k pullups |
| Path-check | button | P0.11 | active low with pull-up |
| Path-check | battery ADC | P0.02/AIN0 | divider sized so input never exceeds VDD |

Power: all logic is 3.3 V. Fixture flow sensor runs at 5 V only if its pulse output is level-shifted/clamped; use open-collector interface. Gateway USB-C 5 V feeds module/regulator rails per module datasheet. Path-check uses a protected cell and a charger/protection design reviewed before production. Add local decoupling per datasheets, reverse-polarity protection, ESD protection at exposed connectors, and test points. No mains circuitry. Battery life is unmeasured.

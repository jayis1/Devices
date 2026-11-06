# Connection schedules (preliminary, not KiCad schematics)

## Window Sentinel
ESP32-C3 module: GPIO4 SDA and GPIO5 SCL to OPT3001 SDA/SCL; 4.7k pullups to 3V3. Sensor VDD 3V3, GND common; I2C address 0x44. Reed switch GPIO6 to GND, internal pull-up enabled; closed = panel present. USB-C 5 V → 3V3 regulator → module and sensor. UART0 GPIO20 TX / GPIO21 RX, EN and BOOT pads. Add input fuse, reverse protection and connector ESD protection in any board implementation.

## Weather Sentinel
Same power and debug arrangement. BME280 at 0x76 (SDO low); VEML7700 at 0x10 share I2C bus GPIO4/5. Verify breakout pull-ups do not parallel below safe resistance. Do not expose sensor to direct rain or solar heating.

## Gateway
Pi Zero 2 W powered by regulated 5V USB, storage on microSD. BLE and Wi-Fi antennas must have module clearance per manufacturer guidance. No mains wiring. These schedules are a concept-level net assignment only; no CAD/ERC/PCB validation.
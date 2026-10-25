# Conceptual connection schedules
Not KiCad schematics; verify module carrier pinouts and design ERC/DRC before fabrication.

route-sentinel: USB5V -> module 5V input; module 3V3 -> VL53L1X VDD; GND common; GPIO6 SDA <-> SDA with 4.7k pull-up to 3V3; GPIO7 SCL <-> SCL with 4.7k pull-up; GPIO4 -> XSHUT. I2C address 0x29.

hub: USB5V -> Pico W VSYS; common GND; GP0 UART TX -> ESP32-C3 RX, GP1 UART RX <- ESP32-C3 TX (3V3 logic); optional GP4/GP5 I2C OLED.

equipment-tag: 2xAA -> nRF52840 module supply per module limits; reed switch between P0.11 and GND, internal pull-up; optional HX711 DOUT P0.26, SCK P0.27, load-cell bridge only via HX711.

climate-node: USB5V -> module input; module 3V3 -> SHT31 VDD; GPIO6 SDA/GPIO7 SCL, common GND, 4.7k pull-ups, address 0x44.

(node weather-node)
  U1 nRF52840 module
  U2 SHT40; U3 BMP390 (I2C)
  BT1 protected 2xAA -> U4 3V3 -> U1/U2/U3
  U1 P0.26 SDA / P0.27 SCL -> U2/U3
  Rain reed input P0.11 with pull-up and debounce

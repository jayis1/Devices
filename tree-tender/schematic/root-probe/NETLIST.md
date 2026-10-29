(node root-probe)
  U1 nRF52840 module
  U2 SHT40
  J1 soil probe -> ADC P0.04
  BT1 protected 2xAA -> U3 3V3 -> U1/U2
  U1 P0.26 SDA / P0.27 SCL -> U2
  GND common; SWD pads retained

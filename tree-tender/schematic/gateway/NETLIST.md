(node gateway)
  U1 ESP32-S3-WROOM-1; J1 USB-C 5V input
  U1 GPIO4 -> D1 status LED through current-limiting resistor
  SW1 user acknowledgement input GPIO0; verify boot strap behavior for chosen carrier
  Antenna keepout per module datasheet; USB ESD protection and decoupling required

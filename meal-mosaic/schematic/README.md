# Reference pin/net schedules (not KiCad schematics)

These are conceptual interfaces only. Per-node schedules: serving and return nodes use GPIO4/5 for HX711 DOUT/SCK and GPIO6 for button; room node uses GPIO4/5 for SHT31 I²C SDA/SCL at 0x44 and GPIO6 for button. Tie sensor grounds to the 3.3 V logic ground, add local decoupling per module datasheets, and expose USB/programming access. Verify GPIO availability and board-specific USB/debug reservations before implementation. No board layout, ERC, enclosure, or electrical validation is supplied.

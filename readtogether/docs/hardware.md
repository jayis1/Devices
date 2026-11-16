# Hardware reference

Prototype part selections (verify lifecycle/stock before ordering):

| Node | SoC / key parts | I/O assumptions | Supply / peak estimate |
|---|---|---|---|
| Family Hub | Raspberry Pi Zero 2 W; microSD; 5 V USB-C supply | Wi-Fi; USB; GPIO UART/I2C | 5 V, allow 2 A supply |
| Book Tag | Nordic nRF52840 module (MDBT50Q-RX); tactile switch; RGB LED; CR2477 holder | GPIO switch/LED; BLE 5 | 3 V coin cell; radio peak depends on module, measure before battery-life claim |
| Reading Dock | ESP32-C3-MINI-1; PN532; tactile button; piezo; RGB LED | I2C SDA GPIO5/SCL GPIO6; IRQ GPIO4; button GPIO3; buzzer GPIO10 | regulated 5 V USB input; 3.3 V logic; piezo driver current TBD |
| Room Beacon | nRF52840 module; 2.13-inch e-paper panel; button | SPI SCK GPIO13/MOSI GPIO15/CS GPIO17/DC GPIO20; button GPIO4 | 3 V battery TBD; peak panel current/driver not budgeted, prototype USB supply recommended |

Values are proposed pin assignments, not electrically validated mappings for a selected breakout; review SoC module pin availability, boot straps and vendor reference circuits before PCB design. Use 3.3 V compatible peripherals only. Do not drive a piezo or LED load beyond GPIO ratings; add transistor/current limiting as appropriate. Battery holder cells require polarity protection and low-voltage cutoff. No chargers are included. Confirm radio region and antenna keep-out from module vendor documentation. BOM files are initial parts lists, not approved purchasing BOMs.

# Context node proposed net schedule (not a KiCad schematic)

ESP32-C3-MINI-1 on regulated 3V3. SHT40 (U2) at 0x44 shares SDA GPIO4 and SCL GPIO5 with 3V3 pull-ups. USB-C input 5V feeds TPS62160-based 3V3 converter; select inductor and capacitors per datasheet. UART0 exposed for debug. Keep sensor thermally isolated from regulator and direct weather exposure.

No native KiCad schematic or PCB is supplied; validate exact module carrier pins, protection and power component values before fabrication. Context node is optional and reports only ambient equipment conditions.

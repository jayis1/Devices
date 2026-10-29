# Pin/net plans — conceptual, not KiCad

All nodes share regulated 3.3 V logic and common ground. Confirm module GPIO mapping and sensor IO voltage before fabrication. No PCB layout is included.

| Node | Net | SoC pin | Endpoint |
|---|---|---|---|
| gateway | SDA/SCL | GPIO8/GPIO9 | SSD1306 I²C (0x3C) |
| soil | ADC | GPIO0 (verify ADC1 on module) | capacitive probe analog out |
| basin | TRIG/ECHO | GPIO4/GPIO5 | ultrasonic; level-shift ECHO if needed |
| rain | TIP | GPIO4 interrupt | reed switch to ground, pull-up |

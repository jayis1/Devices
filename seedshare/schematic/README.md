# Schematic status

These ASCII connection sketches are conceptual wiring aids, not KiCad schematics, PCB layouts, or manufacturing documents. All signals are 3.3 V logic; use regulated 5 V USB input to dev boards.

## Storage node
`ESP32-C3 dev board GPIO8 SDA / GPIO9 SCL ↔ SHT31 breakout (3V3,GND)`
`GPIO2 → status LED + 1kΩ → GND`; onboard USB programming. Use only after verifying GPIO availability on purchased board revision.

## Intake dock
`ESP32-C3 GPIO8/9 I²C ↔ NAU7802 SDA/SCL`; bridge excitation and differential inputs to load cell per ADC/load-cell breakout documentation. `GPIO20 RX/GPIO21 TX ← QR UART TX/RX` (UART voltage must be 3.3 V); `GPIO3/4 ← active-low buttons`; power via USB. Exact pin availability must be checked against board schematic before wiring.

## Gateway
ESP32-S3 dev board powered by 5 V USB. BLE and Wi-Fi use onboard antenna; keep antenna zone clear. Physical enrollment button to an available GPIO with pull-up, status LED through resistor. Exact board revision and GPIO map TBD.

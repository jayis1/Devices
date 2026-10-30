# Architecture and pin map

Reference design only. Target boards: ESP32-C3-DevKitM-1 (3.3 V GPIO) for both leaf nodes and ESP32-S3-DevKitC-1 gateway. Verify exact board revisions before wiring; do not connect unknown 5 V peripherals directly to GPIO.

| Node | GPIO | Interface | Connection / notes |
|---|---:|---|---|
| Bed probe | 0 | ADC | Capacitive soil probe analog output, 0–3.3 V; confirm actual probe range and calibrate wet/dry endpoints |
| Bed probe | 4 | 1-Wire | DS18B20 data with 4.7 kΩ pull-up to 3.3 V; waterproof probe variant and cable not specified |
| Bed probe | 5 | ADC | Battery monitor via 100 kΩ/100 kΩ divider from protected supply; confirm ADC attenuation/calibration |
| Bloom check-in | 2, 3 | GPIO input | Two debounced buttons to ground, internal pull-ups |
| Bloom check-in | 6 | I²C SDA | Optional e-paper driver; exact panel/controller unresolved |
| Bloom check-in | 7 | I²C SCL | Optional e-paper driver; verify address and voltage |
| Site gateway | 8 | GPIO output | Status LED via series resistor |
| Site gateway | 9 | GPIO input | Enrollment button to ground, internal pull-up |
| Site gateway | Wi-Fi/BLE | Radio | ESP32-S3 integrated RF; antenna keep-out per board/module vendor design |

GPIO assignments are a provisional reference and require confirmation against the chosen module, boot straps, ADC behavior and board revision. No PCB-ready schematic is supplied. Power: protected USB 5 V input through dev-board regulation; nominal 3.3 V sensor rail. Estimated sampling budget (assumption only): 50 mA awake for 2 s every 15 min + 0.5 mA sleep average gives ~0.61 mA average; a nominal 2000 mAh power bank cannot be translated directly to runtime due to conversion losses, board sleep current, temperature and self-discharge. Measure before making runtime claims.

Assembly safeguards: avoid exposed conductive probe connections in wet soil; strain-relieve cables; use a weatherproof enclosure with drainage/condensation strategy; keep gateway antenna clear of metal/wet foliage. No outdoor ingress rating is claimed. Reference wiring sheets are under `../schematic/`.

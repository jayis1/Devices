# Architecture and build notes

## Data path
Storage cabinet SHT31 → ESP32-C3 storage node → bonded BLE GATT → ESP32-S3 gateway → SQLite event log → optional MQTT TLS broker and FastAPI clients. Intake dock combines UART QR scan, load-cell ADC and explicit volunteer confirmation. The gateway is the timestamp/sequence authority; on clock loss timestamps are marked uncertain. The 5 kg bridge load cell is mechanically mounted beneath a stable platform; NAU7802 I²C at 3.3 V, SDA GPIO8/SCL GPIO9 on a typical C3 dev board. SHT31 uses I²C at address 0x44, SDA GPIO8/SCL GPIO9. UART scanner: GPIO20 RX/GPIO21 TX at 9600 8N1; buttons GPIO3/4 with pull-ups; HX711/NAU data-ready GPIO5. Verify against the exact purchased board pinout before wiring; these are logical board GPIOs, not module pad numbers.

Gateway uses a vendor ESP32-S3 devkit with BLE/Wi-Fi. Board-specific peripheral mapping is intentionally unassigned until exact revision selection. All supplies are 5 V regulated USB, dev-board 3.3 V rail powers low-current sensors. No battery-life claim. Use short I²C runs, 4.7 kΩ pull-ups to 3.3 V if absent on breakout, local decoupling per component datasheets, strain relief, reverse-polarity protection at custom boards, and ESD protection on exposed USB/UART. Never connect 5 V UART to 3.3 V GPIO.

## Failure and safety
Local event log persists through internet/broker loss. Sensor absence, out-of-range values and stale data are distinct states; RH above configured threshold raises a human advisory, never drives a heater/dehumidifier. On gateway outage, dock queues a bounded set of events in nonvolatile storage and displays fault; volunteers use paper log fallback. Duplicate sequence IDs are idempotent. Physical prototype, exact board datasheets, EMC, accuracy and cabinet uniformity testing remain required.

## BOM cost basis
Planning-only unit costs (USD, approximate, no supplier quote): gateway $18, storage $14, dock $28. See per-node CSV for specific reference designators and alternatives.

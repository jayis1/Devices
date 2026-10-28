# Connection-level schematics

These are pin/net design notes, not KiCad schematics. Confirm board revision and datasheets before wiring; common ESP32-C3 modules vary by carrier.

Gateway (ESP32-S3-WROOM-1 carrier): USB-C 5 V → protected 3V3 regulator → module; GPIO8 SDA/GPIO9 SCL reserved for I²C expansion; GPIO18 LED; BLE radio receives v1 advertisements. Add 100 nF at each IC and bulk 10 µF near module supply. Keep antenna edge clear per Espressif layout guide.

Equipment tag: ESP32-C3 module powered from protected 5 V carrier supply; I²C GPIO4 SDA/GPIO5 SCL to LIS2DH12 (address 0x18, SA0 low); reed switch GPIO3 to ground with internal pull-up and 10 kΩ external pull-up to 3V3. Interrupt INT1 → GPIO2. No direct battery charging circuitry is specified.

Surface sentinel: ESP32-C3; TMP117 I²C address 0x48, SDA GPIO4/SCL GPIO5; ALERT GPIO2. Capacitive electrode connects to GPIO0 through a series 1 kΩ and is measured with a guarded, protected high-impedance input; this is an experimental wetness proxy, not calibrated moisture measurement. Common 3V3, ground, and local 100 nF decoupling.

Inspection beacon: ESP32-C3; button GPIO3 to ground with pull-up and 20 ms debounce; LED GPIO4 through 1 kΩ to ground. LED reports local event state only. No access-control output.

Connector pinout for sensors: 1=3V3, 2=GND, 3=SDA, 4=SCL. Do not attach 5 V I²C devices. Firmware drivers and electrical design remain prototype work.

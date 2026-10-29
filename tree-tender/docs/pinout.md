# Prototype pin assignment

These are proposed carrier-board assignments; verify against the exact module/board revision and SDK before fabrication. All logic is 3.3 V.

| Node | Signal | SoC GPIO | Interface / note |
|---|---|---:|---|
| Probe nRF52840 | I2C SDA/SCL | P0.26/P0.27 | SHT40, pullups to 3V3 |
| Probe nRF52840 | soil ADC | P0.04 (AIN2) | input only, divider/clamp to <=3V3 |
| Probe nRF52840 | battery sense | P0.02 (AIN0) | resistor divider, switched to reduce leakage |
| Probe nRF52840 | SWDIO/SWCLK | dedicated SWD | retain programming pads |
| Weather nRF52840 | I2C SDA/SCL | P0.26/P0.27 | SHT40 + BMP390, check address straps |
| Weather nRF52840 | rain reed | P0.11 | pull-up, debounce, ESD protection |
| Gateway ESP32-S3 | I2C SDA/SCL | GPIO8/GPIO9 | board status expansion only |
| Gateway ESP32-S3 | status LED/button | GPIO4/GPIO0 | ensure board boot-strapping behavior reviewed |

Power: protected 2×AA to low-quiescent 3.3V regulator for sensor nodes; no charging. Gateway USB-C 5V input, regulated by module/carrier. Add decoupling at module and sensors. Probe output range varies by product; measure before connection. Values are design proposals, not datasheet-certified layout.

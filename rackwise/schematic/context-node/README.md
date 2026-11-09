# Weather/context node wiring reference (not a KiCad schematic)

ESP32-C3-MINI-1 module on 3.3 V from isolated 5 V adapter/regulator (500 mA transient capability). BME280 powered at 3.3 V: SDA GPIO4, SCL GPIO5, 4.7 kΩ pull-ups to 3.3 V, address 0x76 (SDO low). GPIO8 status LED via 1 kΩ. Place sensor away from regulator and direct sunlight; use a ventilated radiation shield outdoors. Verify exact sensor/module breakouts and pin mapping against vendor documentation before assembly. No weatherproof enclosure or calibration is included.

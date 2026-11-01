# Architecture (DRAFT)

```text
Marker nodes (ESP32-C3 + VL53L0X) --BLE adv--> Gateway (Pico W + USB BLE adapter)
Staff beacon (ESP32-C3 button) -------BLE----> Gateway --Wi-Fi/MQTT TLS--> API
```

The marker node observes a fixed installation; the staff beacon supplies a deliberate confirmation. Gateway coordinates clocking, deduplication, local queue and upstream delivery. No cloud is needed for sensing or manual inspection. Internet loss: queue events; gateway loss: marker nodes advertise but no aggregation, use normal human inspection/reporting. Recover by replaying event IDs; consumers deduplicate.

Power: gateway certified isolated 5 V USB. Marker target 2×AA; sleep current, radio duty cycle, regulator losses and lifetime are TBD pending prototype measurements. BLE advertisements should be sparse, with sensor wake on scheduled interval/button. Do not claim a battery life. No mains or actuators.

Per-node net plan: marker ESP32-C3 3V3/GND; I2C GPIO4 SDA/GPIO5 SCL to VL53L0X (address 0x29, one sensor per bus); GPIO6 button to ground with pull-up. Inspection beacon GPIO4 button, GPIO5 LED through resistor. Add local decoupling, ESD protection at exposed contacts, programming pads, antenna keepout, battery protection and low-voltage cutoff in reviewed schematic. Pico W USB power; its wireless design does not guarantee BLE scanning, so external USB BLE adapter is required. Exact adapter driver remains TBD.

No KiCad files are claimed. This text is a preliminary pin/net plan, not a fabrication schematic. Check module pin mux, supply ranges, pull-ups, enclosure, weathering, and radio regulations before layout. Hardware and RF not tested.
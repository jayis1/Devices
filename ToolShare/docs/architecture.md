# ToolShare architecture

```text
Shelf scales (ESP32-C6 + HX711) ─┐
Shelf scales (ESP32-C6 + HX711) ─┼─ BLE ─ Pi Zero 2 W hub ─ SQLite/FastAPI ─ optional MQTT sync
Intake station (ESP32-C6 + PN532)┘
```

Node reports are advisory. Shelf scale weight maps to a configured bay and approximate availability; it cannot identify a specific tool among multiple objects. Intake station records staff-initiated asset events. Hub stores bounded local history and exposes read endpoints. Repeated messages are idempotently keyed by node ID and sequence. Internet failure does not disable local API. Hub failure leaves nodes reporting after restart; node event buffering is a future SDK integration task.

Threat boundaries: BLE radio and optional MQTT are untrusted transport. Current codec CRC is error detection only. Prototype must remain on a trusted network until application-layer authentication, authorization, replay defense, signed updates, and secure credential provisioning are implemented. No personal information is collected by design.

Power: USB-powered nodes/hub. No battery-life estimate is applicable. Verify each breakout's IO voltage, load-cell capacity, regulator thermal limits, and cable protection in the actual build. Electrical pin mapping is a design note, not a reviewed schematic.

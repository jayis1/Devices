# Architecture and prototype decisions

## Roles and data flow

| Node | SoC / role | Sensors / action | Link |
|---|---|---|---|
| Gateway | ESP32-S3-WROOM-1; BLE central + MQTT uplink | Packet validation and bounded offline queue | BLE 5.0, Wi-Fi TLS |
| Equipment tag | ESP32-C3-MINI-1 | LIS2DH12 motion, reed contact | BLE advertisement |
| Surface sentinel | ESP32-C3-MINI-1 | TMP117 temperature, capacitive wetness electrode | BLE advertisement |
| Staff beacon | ESP32-C3-MINI-1 | Debounced acknowledge button, status LED | BLE advertisement |
| Site service | Linux host/Python | MQTT, SQLite, HTTP API | MQTT/TLS + REST |

Pico W is an optional local display/controller in the concept; the initial vertical slice uses the ESP32-S3 gateway because BLE central support and Wi-Fi are both available on one module. Do not treat the Pico W as implemented in this prototype.

## Pilot sequence

At 30 s cadence, sensor nodes advertise a packed frame. Gateway rejects wrong versions, invalid CRC, unknown node IDs, and stale sequence numbers; it forwards accepted telemetry to MQTT topic `parkplay/v1/<site>/<node>/telemetry`. Backend stores bounded JSON payloads and returns the latest reading per node. When internet is unavailable, gateway should buffer at most 256 readings and discard oldest telemetry first; inspection acknowledgements must be visibly marked pending until confirmed. This firmware queue is a design requirement, not implemented in the example code.

## Failure behavior

| Failure | Detection | User-visible behavior | Recovery |
|---|---|---|---|
| Node silent > 3 intervals | Gateway freshness timer | Unknown/stale; request human inspection if relevant | Check battery, mount, radio |
| Sensor invalid/out of range | Range and bus checks | Unknown, never safe | Inspect connector and recalibrate |
| Gateway offline | API heartbeat absent | Site marked disconnected | Restore power/network; reconcile sequence IDs |
| Broker/cloud unavailable | MQTT reconnect/backoff | Local display continues; remote status stale | Retry TLS session and drain queue |
| Clock rollback | Monotonic sequence + receive time | Timestamp quality flagged | Re-sync gateway time |
| Staff beacon event repeated | Event ID deduplication required | One inspection event | Confirm server acknowledgement |

## Power and installation

Prototype modules are USB-powered, with fused 5 V input and onboard regulation. Field deployment requires measured startup/peak current, regulator thermal verification, protected rechargeable supply design, UV-rated enclosure, cable glands and mounting review. Sensor nodes must be mounted so no sharp edge, pinch point, or loose part is introduced. Antenna keep-out and jurisdictional RF requirements follow module documentation. No runtime or ingress rating is claimed.

## Trust boundaries and data minimization

BLE telemetry is observable unless link security is provisioned; prototype frames have CRC for accidental corruption, not authenticity. Production must use unique per-node credentials, authenticated pairing, replay protection, TLS MQTT, broker ACLs, authenticated API users, rate limits, and signed updates. No credentials or private keys belong in firmware source. Store coarse readings only; no personal identifiers, raw media, or child-level tracking. Local administrator defines retention and deletion.

## Validation status

The host protocol helper can be compiled/tested without target hardware. ESP-IDF target builds, sensor-driver integration, physical BLE range, RF coexistence, enclosure, environmental, and safety validation are outstanding. These documents specify a prototype, not a certified playground inspection product.

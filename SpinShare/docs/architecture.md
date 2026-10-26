# Architecture

Two battery sensor nodes sample an LIS3DH at 25 Hz and use a reed switch as corroborating door evidence. A Pi Zero 2 W gateway scans/provisions BLE nodes, stores only latest state in RAM, serves LAN HTTP, and optionally publishes coarse MQTT telemetry. Nodes retain sequence and monotonic uptime; gateway assigns receipt time and marks reports stale after 90 seconds. No actuator commands are supported.

Node responsibilities: sensor node = sampling, debounce, threshold/hysteresis, BLE telemetry; gateway = aggregation, freshness, local API; phone = display and refresh only. Washer and dryer are identical hardware with installer-assigned roles. A single sensor cannot cover separate machine enclosures. Local operation does not depend on internet or MQTT.

Power: sensor node uses protected 3.7 V Li-ion, low-Iq 3.3 V regulator and USB charging module with cell NTC; target sleep <20 uA, BLE/reporting average <1 mA, 1000 mAh gives rough >40 days before derating. Validate on hardware. Gateway is 5 V USB-C, budget 5 W. Do not mount inside appliances. BLE 2.4 GHz coexistence and enclosure placement require site testing.

Privacy: no occupants or machine contents are observed. Keep BLE pairing allowlisted; rotate/rescope site credentials. Optional MQTT uses TLS and broker ACLs. RAM-only cache by default. Loss of gateway/cloud results in `offline`, not `available`.

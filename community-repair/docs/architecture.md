# Architecture

Tool Dock detects presence of common tethered hand tools with switch contacts. Pump Monitor measures pressure response only during a staff/operator initiated manual test; no motorized pumping or valves. The ESP32-C6 gateway aggregates BLE telemetry and can serve an operator's local view during WAN loss. Optional cloud forwarding is TLS-protected MQTT; API data is advisory and staff-confirmed. Sensor failures and stale reports map to unknown. The system is not a bicycle inspection or safety certification.

BLE was selected for the short-range, low-rate, battery-powered nodes. Validate co-existence and range at each metal stand. Nodes do not use Wi-Fi. Gateway requires USB supply. Node battery life values in the top-level README are targets derived from assumed current, not measurements.

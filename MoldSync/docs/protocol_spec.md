# MoldSync Protocol Spec

## Transport
- Primary: Sub-GHz 868 MHz TDMA star/mesh hybrid
- Commissioning: BLE 5.0 on mobile-facing nodes
- Hub uplink: Ethernet/Wi-Fi with MQTT over TLS

## Message types
- `0x01` telemetry
- `0x02` event
- `0x03` alert
- `0x04` command
- `0x05` ack
- `0x06` config
- `0x07` ota chunk

## Command examples
- `set_sampling_profile:bath_high_humidity`
- `run_vent:1200`
- `close_valve:branch_utility`
- `capture_inspection:bathroom-east`

## Telemetry payload conventions
- temperatures in centi-degrees C
- RH in tenths of percent
- flow in mL/min
- capacitance feature in normalized pF delta
- battery in mV

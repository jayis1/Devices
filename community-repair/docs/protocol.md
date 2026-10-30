# Protocol

BLE GATT service UUID 8a2f0001-0b7a-4d9e-a3c1-5ad1b2c00001. JSON telemetry max 160 UTF-8 bytes, v=1, node identifier, monotonically increasing uint32 sequence and kind. `tool_state` carries tool/present/battery_mv; `pump_test` carries integer pressure_kpa; heartbeat carries battery_mv. Retransmit latest state on reconnect; gateway de-duplicates node + sequence and flags counter gaps. Timestamps are gateway receipt time, not sensor time. No remote actuation. Advertisement/GATT security as currently specified is prototype-grade only; production requires bonding and authenticated enrollment.

The separate `firmware/common` CRC frame is a portable host-tested binary envelope for future transport integration; firmware stubs do not yet use it or implement BLE encoding.

# CivicCharge protocol v1

Transport: BLE GATT (prototype transport adapter pending); a monitor reports to its paired gateway. Maximum decoded payload 128 bytes. Integer fields are unsigned little-endian except signed `temp_c_centi`.

| Field | Type | Unit / constraint |
|---|---|---|
| v | uint8 | protocol version, 1 |
| node_id | 8 bytes | provisioned stable identifier |
| seq | uint32 | increasing per node; duplicates discarded |
| uptime_s | uint32 | seconds since boot |
| voltage_v_rms | uint16 | centivolts; 0–30000 |
| current_ma | uint16 | mA; 0–30000 |
| temp_c_centi | int16 | 0.01 °C |
| flags | uint16 | bit0 valid, bit1 service requested, bit2 enclosure open |

Reject unknown version, truncated/oversized frames, invalid ranges, and repeated sequence. Gateway reception timestamp is authoritative; sensor wall clock is not used. No actuator commands. Production transport requires authenticated pairing, per-device enrollment, replay-resistant sequence persistence, and a formally assigned GATT UUID; these are not implemented.

Example decoded values: `{v:1,node_id:"0102030405060708",seq:42,uptime_s:3600,voltage_v_rms:12000,current_ma:850,temp_c_centi:2430,flags:1}` means 120.00 V RMS, 850 mA and 24.30 °C. This human-readable example is not a byte-level CBOR vector; codec/transport implementation remains pending.
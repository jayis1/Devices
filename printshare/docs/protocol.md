# Wire and MQTT protocol

Frame is packed little-endian, exactly 18 bytes: `version:u8=1, type:u8, node_id:u16, sequence:u16, uptime_s:u32, state:u8, humidity_centi_rh:u16, temperature_centi_c:i16, battery_mv:u16, crc16:u16`. CRC-16/CCITT-FALSE covers bytes 0..15 (poly 0x1021, init 0xffff, no reflection/xorout). Types: 1 printer observer; 2 filament sentinel. Printer states: 0 unknown, 1 idle, 2 printing, 3 paused, 4 error. Sentinel state: bit 0 dry-box door open; humidity and temperature are centi-units. Unused readings are zero.

BLE deployment requires unique provisioned node IDs and encrypted connections; this portable codec does not implement pairing or cryptographic authentication. Do not use CRC as authentication. Never issue printer commands over this protocol.

MQTT topic: `printshare/v1/<site>/<node_id>/telemetry`; payload JSON keys are `version,type,node_id,sequence,uptime_s,state,humidity_centi_rh,temperature_centi_c,battery_mv`. QoS 1, retain false. Broker ACLs must scope each gateway to its site. Production gateway should deduplicate node_id+sequence and mark state stale after 600 seconds. Reboots may reset uptime; sequence is modular.

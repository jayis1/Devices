# Node protocol v1

Transport target: BLE GATT, local site only. BLE link encryption/bonding is required by product design but not implemented by the host sample. Do not treat CRC as authentication. Production enrollment must use per-site authenticated physical pairing and rotate/revoke node credentials.

Binary frame, little-endian: `magic:u16=0x5252 | version:u8=1 | type:u8 | node_id:u32 | sequence:u32 | value_mg:i32 | flags:u8 | crc16:u16`. CRC-16/CCITT-FALSE covers all preceding bytes; encoded size is 19 bytes. Types: 1=WEIGHT_SAMPLE (value_mg), 2=CONTACT (flags bit0 closed), 3=STAFF_CONFIRM (flags bit0 confirmed), 4=HEARTBEAT. `node_id` is provisioned non-secret identity; sequence increases per boot/session. Hub rejects wrong version, invalid CRC, unknown type, and duplicate event key `(node_id,sequence)`; reboot sequence persistence is not implemented, therefore production must add durable monotonic counters or a random session nonce.

Each link operation has a 2-second timeout, at most three retries with jitter; hub acknowledgement names node and sequence. Stale sensor state is marked offline after 5 minutes. Commands are not part of v1. Maximum frame length is fixed, parser accepts exactly 19 bytes.

# MaintainSync radio protocol

All multi-byte fields are little-endian. The packed frame is `version:u8,type:u8,node_id:u32,seq:u32,epoch:u32,length:u8,payload[96],crc32c:u32,tag[16]`. CRC-32C covers fields before `crc32c`; tag is HMAC-SHA256 truncated to 16 bytes over the complete pre-tag frame. Reject unknown versions, length >96, bad CRC/tag, sequence rollback, and commands past `epoch + ttl_s`.

Types: `1 HEARTBEAT`, `2 TELEMETRY`, `3 INSPECTION`, `16 COMMAND`, `17 ACK`, `18 FAULT`, `32 TIME_BEACON`. Commands encode command ID, nonce, expiry, requested state, and policy revision. Command ACK includes actual output state, key/STOP inputs, and fault bitmap. The hub retries at most twice; nodes never replay an actuation after reboot.

TDMA duty cycle and frequency plan must be built for the installation region. Radio is not an emergency path; the Utility Sentinel must fail safe without it.

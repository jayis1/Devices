# Wire format and API

Wire v1 is 13 bytes: offsets 0-1 ASCII `LL`; 2 version=1; 3-4 node_id u16 BE; 5-8 monotonically increasing sequence u32 BE; 9 flags (bit0 room transmitter_on); 10-11 battery_mV u16 BE; 12 CRC-8/ATM over bytes 0..11 (poly 0x07, init 0). Unknown flag bits are ignored. Reject malformed length/magic/version/checksum and non-increasing sequence. No commands are carried.

API readings are JSON `{ "site":"demo", "room":"main", "sequence":1, "transmitter_on":true, "receiver_count":4, "observed_at":"2026-01-01T12:00:00Z" }`. Site/room are bounded identifiers, count 0..8, sequence >=0; stale >120 seconds is unknown. MQTT requires TLS and broker ACLs. API demo has no authentication and must remain on localhost. No personal information is accepted.

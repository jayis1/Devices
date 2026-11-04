# ChoreCue logical event protocol v1

This is a logical JSON contract, not a finalized BLE wire format. Maximum encoded payload: 192 UTF-8 bytes. Required fields: `v` integer 1; `node` enrolled identifier, 1–48 ASCII characters; `seq` unsigned 32-bit event counter; `type`; `value`; `unit`; `utc_ms` unsigned 64-bit, where zero means unknown.

| Type | Value | Unit |
|---|---:|---|
| task_done | 0 or 1 | bool |
| supply_g | 0..100000 | g |
| availability | 0..2 | enum |

Availability mapping: 0 quiet, 1 available, 2 do-not-disturb. Reject invalid JSON, missing/extra unsupported type values, unknown version/type, non-integral numeric fields, invalid node ID, out-of-range values, wrong unit, or oversized payload. Gateway deduplication key is `(enrolled_node_id, seq)`. Duplicate events never create duplicate completions. Gateway receipt time is authoritative when `utc_ms=0`; sender time is informational until clock synchronization is implemented. Counter rollover requires physical re-enrollment/reset.

No commands are defined. BLE security, pairing, sequence persistence, fragmentation, retry schedule, and OTA are not implemented; do not operate this protocol over untrusted radio as an authenticated system. CRC, if added for transport corruption, is not authentication. Any production extension must use a new version and retain backward-compatibility tests.

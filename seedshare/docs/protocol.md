# Protocol and API contract

Wire envelope: byte 0 version=1; byte 1 type (1 telemetry, 2 inventory event); bytes 2–3 node ID little-endian; bytes 4–7 sequence uint32 LE; bytes 8–9 payload length uint16 LE (max 128); payload; final two bytes CRC-16/CCITT-FALSE over preceding bytes, little-endian. Reject wrong version, truncated/oversized frames and CRC errors. Sequence numbers are monotonic per node; duplicates ignored, gaps surfaced. BLE encrypted bonding; physical gateway-button authorization; rotate/revoke bonds on volunteer reset. CRC detects corruption, not authentication: BLE link encryption is required and MQTT uses TLS. No custom cryptography.

MQTT telemetry topic: `seedshare/v1/<site>/<node>/telemetry`; JSON payload `{version:1,node_id:string,sequence:int,kind:string,ts_utc:string|null,values:{...}}`. Backend endpoint `POST /api/v1/events` accepts the same fields, rejects unknown fields and non-finite values. `GET /api/v1/events?limit=100` returns newest first (limit 1–500). `GET /health` is liveness only. MQTT credentials must be site-scoped; broker ACL limits each gateway to its own site prefix. The example adapter is not an authentication provider; deploy behind TLS and site authentication before network exposure.

Humidity values are %RH, temperature °C, mass grams; event identity is a volunteer-confirmed opaque packet code. No identity fields for borrowers.

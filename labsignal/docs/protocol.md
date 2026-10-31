# Wire and API contract

Frame is exactly 12 octets: offset 0 version=1; 1 kind (1 sensor, 2 dock); 2..5 sequence uint32 little-endian; 6..9 signed int32 value in milli-units, little-endian two's complement; 10..11 CRC-16/CCITT-FALSE little-endian over bytes 0..9 (poly 0x1021, init 0xffff, xorout 0). Reject wrong length/version/kind/CRC. Sequence monotonic per node, wrap comparison must use serial arithmetic; deduplicate within bounded node window. No timestamp in binary format; gateway receipt time only, explicitly not sensor time.

MQTT optional topic exactly `labsignal/v1/{site_id}/{node_id}/telemetry`; payload JSON keys `v`, `kind`, `seq`, `value_milli`, `received_at`. No retained command topics. HTTPS API prototype: `GET /health`; `POST /events` with JSON event; enforce limits/auth at deployment edge (not currently implemented). CRC is not authentication.

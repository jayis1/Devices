# REST / MQTT API

`GET /health` returns `{"status":"ok"}`. `GET /sites/{site_id}` returns latest validated sample or 404. `POST /telemetry` accepts JSON `{site_id,node_id,sequence,observed_at,kind,values}`; production deployment must authenticate the gateway and enforce site ownership, size/range limits, freshness, and replay rejection. Current demo has no authentication and must bind only to a trusted local network.

MQTT topic when enabled: `heat-relief/v1/{site_id}/{node_id}/telemetry`; telemetry only, no retained commands. Configure TLS and broker ACLs. Payload schema follows docs/protocol.md semantics. No production broker connectivity is implemented in this starter.

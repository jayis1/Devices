# Local API contract

`POST /v1/events`: JSON fields `{version,event_type,node_id,sequence,ticket_id,state}`. Numeric bounds and schema are enforced by Pydantic. Duplicate node/sequence returns HTTP 409. `GET /v1/events?limit=100`: returns newest-first event objects and server receive timestamps; limit must be 1..500. This API is a local demo only: no auth, TLS, authorization, MQTT, retention/delete endpoint, or rate limiting. Bind to 127.0.0.1 only; do not deploy exposed.

Intended future MQTT topics: `repaircafe/v1/<site>/<node>/events` (node publish only), `repaircafe/v1/<site>/status` (gateway aggregate). This is documentation, not implemented. Production requires TLS, per-node ACL, enrollment, rotation/revocation and consented site identifiers.

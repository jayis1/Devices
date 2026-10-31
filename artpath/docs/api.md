# Local API contract

`GET /health` — local process health and prototype mode.

`GET /content/{content_id}` — currently returns unavailable placeholder; no public-domain or historical text is invented by the service.

`POST /readiness` — validates protocol v1 readiness event: node id (1–64 chars), uint32 sequence, lowercase 128-bit hex event ID, content revision, max 16 fault labels. Stores latest equipment state in local SQLite. Same sequence from a node is treated as duplicate. No authentication is implemented; loopback-only use is required. Data is equipment readiness, not visitor activity.

MQTT/cloud transport is not part of this release. Production exposure requires authentication, TLS, authorization, validated event deduplication, rate limiting, retention and backup policy.

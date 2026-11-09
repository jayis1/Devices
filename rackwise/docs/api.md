# Read-only API prototype

`GET /health` returns `{"status":"ok"}`. `GET /v1/sites/{site_id}/availability` returns an availability band, coarse free count, gateway-generated timestamp, and stale flag. Prototype data is static and illustrative; it does not yet subscribe to MQTT or provide authentication. Bind to loopback for development. Before any network exposure add identity, site authorization, TLS termination, rate limiting, and persistent retention enforcement.

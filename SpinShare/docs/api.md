# Local API

`GET /health` returns `{"ok":true}`. `GET /v1/machines` returns latest machine status, `updated_age_s`, and `state` (`running`, `possibly_done`, `available`, `idle_unknown`, `offline`). Stale records are offline. Demo app is unauthenticated and must bind to localhost/trusted LAN only; do not expose to the internet. Production needs HTTPS, authentication, authorization, persistence/retention policy, rate limits, and MQTT topic ACLs.

# API

`POST /telemetry` accepts application/json with required fields `version=1,type,node_id,sequence,uptime_s,state,humidity_centi_rh,temperature_centi_c,battery_mv`. Integers are range checked; malformed data returns 422. This demo has no persistence and no authorization; bind to loopback only. `GET /health` returns `{"ok":true}`. Production deployment must add auth, TLS termination, retention/deletion policy, and MQTT adapter with ACL enforcement.

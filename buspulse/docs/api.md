# Local API

Author: jayis1. Prototype routes: `GET /health`, `POST /telemetry` JSON `{node_id,sequence,timestamp,value_mv,temperature_centi_c,valid}`, `GET /nodes`. In-memory only; data disappears on restart. No MQTT client or authentication is implemented. Bind to loopback/trusted lab network; never expose publicly. Production requires TLS, authorization, retention controls, broker ACLs and tested device ownership checks.

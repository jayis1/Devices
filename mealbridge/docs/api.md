# API

Run from this directory: `uvicorn main:app --host 127.0.0.1 --port 8000`. `GET /health`; `POST /telemetry` with `{v:1,node_id,seq,ts_utc,kind,value,unit}`; `GET /telemetry?limit=100`. Duplicate `(node_id,seq)` is idempotently rejected. SQLite file is local. Authentication, tenant authorization and MQTT bridge are not implemented: do not expose this prototype to a network or use real household records.

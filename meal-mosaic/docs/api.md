# API

`POST /observations` accepts JSON `{node_id,kind,sequence,value,status}` and returns `accepted`. `GET /observations` returns up to 100 newest rows. Kinds and units are in `docs/protocol.md`; API timestamps are gateway receive time. Duplicate node/sequence returns HTTP 409; invalid fields return 422. Prototype stores SQLite beside `app.py`. No MQTT, auth, user accounts, role checks, retention deletion or production deployment is implemented. Bind only to localhost for demonstration.

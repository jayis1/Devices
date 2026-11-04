# API reference

Prototype `POST /events` accepts JSON `{version,node_id,seq,kind,value,unit,utc_ms}` matching `docs/protocol.md`. Current endpoint validates schema and ranges; production deployment must add authentication, tenant/site authorization, rate and payload limits, retention/deletion controls, and TLS termination. MQTT topic proposal: `drycycle/<site_id>/events`; broker TLS and per-device ACLs are required, with no anonymous access. Mobile app API integration is not implemented.

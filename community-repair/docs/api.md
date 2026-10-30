# API and MQTT

Read-only prototype endpoints: GET `/health`; GET `/sites/{site_id}/status` returns `unknown` until a verified backend is configured; GET `/sites/{site_id}/events` returns empty list. Optional telemetry topic: `communityrepair/v1/{site_id}/{node_id}/telemetry`. Never publish credentials. MQTT ingestion/storage is not implemented in this prototype; status deliberately does not infer availability.

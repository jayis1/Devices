# Architecture

GL-2 and GL-3 measure ambient light at separate route points and advertise compact readings over BLE. GL-1 collects, timestamps and exposes latest readings locally; optional site deployment may relay validated events over authenticated MQTT to the FastAPI service. Mobile client is a read-only display. Gateway-local inspection remains possible without WAN; this initial software does not implement durable offline buffering. No control outputs exist.

Node IDs are provisioned labels, not cryptographic identities. Production deployment must add authenticated enrollment, per-device credentials, TLS, replay windows, durable queue limits, retention/deletion and signed OTA before handling public infrastructure data. No camera or personal data is collected.

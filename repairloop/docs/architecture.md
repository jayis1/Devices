# Architecture and operations

Gateway hosts FastAPI and SQLite on venue LAN; bench beacons provide explicit state; optional ticket dock requests ticket allocation. SQLite commit is authoritative. Queue scheduling is FIFO among category-compatible tickets; no automated matching or ML inference is required. Offline nodes show unknown after a configurable 30-second freshness window. Gateway outage requires paper/manual queue and does not silently accept tickets. Close event deletes rows after operator confirmation/export.

Deployment: bind API only to the trusted venue interface behind local firewall; do not port-forward. Add operator authentication and TLS before multi-user or remote exposure (prototype API has no authentication). MQTT integration is a deployment interface specification, not implemented in this code. Do not use real visitor information.

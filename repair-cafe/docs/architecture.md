# System architecture and operational assumptions

Intake dock emits an anonymous ticket-created event. Volunteers can then use a printed random ticket code; no identity or free-text personal information is stored. Bench beacons report staff-declared ready/busy/paused state. Gateway is the only coordinator; cloud is optional. FIFO based on local receive order is the baseline. Local paper fallback is mandatory during gateway failure.

Failure policy: stale beacon => unknown; network/cloud failure => local operation; gateway/power loss => paper queue; corrupt or duplicate frame => reject and log counters; full persistent queue => visible alarm and operator export/paper procedure. The demo does not yet implement durable event journal, queue cap, health heartbeat or BLE transport.

Security boundary: API binds localhost for demo. Prototype BLE is unauthenticated; no production credentials embedded. Production threat model includes device replacement, malicious event injection, replay, broker compromise and lost gateway. Require enrollment, TLS/ACLs, signed update and data purge before deployment.

Power: all modules USB 5V supplied, regulated to 3.3V where needed; no battery runtime claim. LDO sizing, transient current, thermal behavior, ESD and enclosure requirements remain to be validated against selected board and environment. No electrical connection to repair targets.

# WorkshopSync architecture

Author: jayis1

## Boundaries

The system observes workshop conditions and provides local guidance. It does not form any part of a machine's safety-rated circuit and must not be wired in series with an emergency stop, guard, motor controller, or mains conductor.

## Data path

Endpoints sample locally and send compact feature telemetry in assigned TDMA slots. The hub verifies protocol version, CRC, length, sequence progression, and freshness before storing a normalized event. A readiness rule is explainable: every card lists the source node, measurement time, quality flag, threshold/baseline, and recommended manual response.

## Storage and resilience

The hub retains a bounded SQLite event store. Endpoints retain only their last 32 telemetry records. On reconnect, telemetry is marked with original capture time; the server makes ingestion idempotent by `(node_id, seq)`. The hub has monotonic-time fallback after RTC recovery and never sends a command without an expiry. A broker outage cannot prevent local display or sensing.

## Security and privacy

Use one identity/key per node, TLS for broker/API links, least-privilege broker ACLs, a short-lived enrollment channel, and rotating deployment credentials. No raw audio, video, workshop location, or personal identity is required. Export/delete requests are handled by the owner of the local hub. Never commit keys, certificates, broker URLs with credentials, or real telemetry.

## Power budget assumptions

Hub: 5 V, 2.5 A peak, 3 A eFuse. Tool Dock: 12 V, 250 mA peak including radio bursts. Air Sentinel: 5 V, 500 mA peak during particulate fan pulses. PPE Tag: 3.0 V, 18 mA radio burst; duty-cycled design target only. Bench Mat: 5 V, 180 mA peak. Runtime and RF range require physical validation.

## Versioning

Protocol and API versions start at 1. New optional fields may be added; removed or reinterpreted fields require a version increment. Hubs reject newer mandatory protocol versions and show a compatibility card rather than guessing.
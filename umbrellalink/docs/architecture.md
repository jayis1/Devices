# Architecture and safety

UmbrellaLink is an anonymous neighborhood umbrella availability system. It has one Linux gateway, one lending dock and one return dock. The dock nodes use four normally-open slot switches each; no cameras, identity tokens, locks, or user accounts are present. The gateway forwards aggregate slot state only. A staff member inspects and replenishes inventory.

## Availability semantics

A reading reports free and total slots for one dock. The API reports each dock independently, plus the latest observation timestamp and an explicit stale flag. Do not infer number of umbrellas available at the whole site by adding lending and return capacity: docks represent different physical racks. Duplicate sequence numbers are ignored. Sequence wrap uses unsigned 32-bit serial arithmetic; re-enrollment resets sequence only after clearing gateway cache. A radio CRC detects accidental corruption, not malicious packets. Until message authentication and controlled enrollment exist, isolate the prototype and do not expose it as trusted infrastructure.

## Reliability and threat model

On radio failure nodes retry three times and retain latest state; gateway marks the node stale rather than inventing availability. Gateway queues only latest per-node state on WAN outage. Restart restores cache from local durable state in the future implementation; this persistence is not implemented yet. Switch bounce is filtered in hardware and firmware. Sensor disagreement or impossible count is rejected and surfaced for staff review. No actuators exist, so there is no autonomous lock or dangerous fallback.

Data minimization: no person-level event stream, audio, image, or location data. Production deployment still needs authenticated MQTT clients, per-topic ACLs, API rate limiting, TLS certificate rotation, signed node frames, retention policy, and site operator consent. Do not put credentials in the repository.

## Operator checklist

Confirm local frequency regulations and antenna requirements; use authorized 868 MHz settings only where legal. Verify 3.3 V levels and regulator current margin. Test stale behavior with WAN disconnected; test malformed/duplicate/out-of-order packets; inspect switch count against physical inventory. Enclosure weather resistance, accessibility, vandal resistance, and public liability require field-specific review. Do not claim availability guarantees.

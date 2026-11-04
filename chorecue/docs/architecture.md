# Architecture and verification plan

## Roles

- Home hub: enrollment, deduplication, canonical household task state, local API, optional MQTT forwarding.
- Task buttons: explicit, user-triggered completion events. A button maps to a configured task and may be anonymous.
- Supply dock: coarse load-change observations for household-designated consumables; a human confirms reorder/refill.
- Room beacon: manually selected availability state; no occupancy detection.

Events flow node → hub → local database/API → app. Internet/MQTT is optional and not on the primary path. Nodes do not receive actuator commands. On hub loss, nodes indicate disconnected state and retain at most a small bounded event queue; recovery reports queued events with sequence IDs for hub deduplication. If queue fills, the node signals failure instead of overwriting silently. The reference firmware currently implements only event validation, not this radio/queue behavior.

## Trust boundaries

Prototype BLE advertisements and local HTTP are untrusted. Do not expose the prototype API beyond loopback/LAN. Production needs secure pairing, per-node identity, revocation, replay protection, encrypted transport, household authorization, TLS with certificate verification, least-privilege broker ACLs, and protected secrets. No credentials belong in source control. MQTT is optional and telemetry must not be retained. Cloud sync is opt-in and requires consent and deletion/export controls.

## Failure and safety behavior

Malformed or out-of-range events are rejected. Duplicates are idempotently discarded. Old or unknown version events are rejected. Hub downtime blocks recording; show explicit disconnected status rather than imply completion. Missing supply data is “unknown,” never “empty.” Clock uncertainty is represented by hub receipt time. No output controls appliances or safety-critical functions. An inaccurate supply estimate cannot trigger an order or physical action.

## Verification matrix

| Test | Required result | Current evidence |
|---|---|---|
| Bounds/version/unit validation | Reject invalid payloads; accept valid events | Host unit test for protocol C library |
| Duplicate and out-of-order events | Deduplicate by enrolled node and sequence; old events do not roll state backward | Specified; integration test pending |
| Hub/network loss | Local behavior and explicit stale state | Specified; hardware test pending |
| MQTT auth / household authorization | Reject unauthorized node/user | Not implemented |
| Radio, battery and RF coexistence | Measured runtime/range and loss behavior | Not performed |
| Enclosure and accessibility | Usability and electrical review | Not performed |

No compliance, fairness, accuracy, reliability, or battery-life claims are made. Before deployment, conduct household consent/usability review, threat modeling, electrical review, and physical tests. Native KiCad schematics/ERC and node radio firmware are future work.

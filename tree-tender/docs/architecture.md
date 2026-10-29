# Architecture and design rationale

## Candidate selection

Selected: street-tree stewardship coordination. Alternatives considered: apartment air quality (overlaps BreathHome/MoldSync), home water leak control (FlowGuard), and community cooling-site readiness (Heat Relief Network/CoolStop). TreeTender differs by serving public-tree volunteers and municipal forestry, with distributed root-zone observations linked to human-confirmed watering visits; it does not control irrigation.

## Requirements

- Collect location-independent node ID, timestamp, soil-moisture proxy and weather observations from several nodes.
- Mark missing/stale data; retain local data through WAN outage.
- Present a human reviewable priority list and visit confirmation.
- No autonomous watering or health/safety claims.
- Design target: 30-minute sampling; household-scale BLE coverage to gateway must be measured at installation.

## Data path

nRF52840 node samples SHT40 and capacitive probe/BMP390, assigns monotonic sequence and emits BLE advertisement. ESP32-S3 gateway validates basic ranges, deduplicates by node+sequence, persists local queue, and forwards MQTT TLS telemetry. FastAPI validates the same observation schema and stores SQLite records. Mobile stub reads REST data and posts steward visit notes. Internet loss never interrupts local sampling; the prototype does not provide encrypted BLE identity.

## Failure and privacy

| Failure | Behavior |
|---|---|
| Probe unplugged/out of range | fault/stale state; never substitute zero |
| Gateway restart | persist pending queue; resume forwarding |
| WAN/broker outage | bounded local queue, retry with backoff |
| Full storage | stop oldest-first telemetry retention; alert operator |
| Clock unavailable | preserve sequence; timestamp flagged uncertain |
| Bad/range-invalid values | reject and log count without raw secrets |
| Battery low | report low-battery status; schedule human service |

No personal tracking, image/audio, or control commands. Default telemetry retention 90 days; site admin deletion/export is required for deployment.

## Readiness

Concept plus host API prototype only. No firmware SDK build, KiCad ERC, RF test, field validation, or measured power test has been performed. Physical installation requires site approval and weatherproof enclosure design. No claims of tree-health diagnosis or safe-route status.

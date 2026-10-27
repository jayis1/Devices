# Architecture and decisions

Invented and maintained by jayis1. Prototype readiness: conceptual wiring and source scaffolds; no boards assembled or hardware validated.

## Rationale and boundaries

CupCycle addresses staff coordination at reusable-cup venues, not consumer kitchen automation. Return chute sensing needs a physical beam at the return aperture; wash rack inspection is a separate manual event at the wash area; hub provides shared aggregate state and offline coordination. These distinct locations justify three nodes. ML is unwarranted for a beam-break count and explicit human inspection.

## Trust and data flow

Chute increments a saturating batch counter on a debounced clear-to-blocked-to-clear cycle. Staff performs a physical count reconciliation and can reset the batch at shift change. Rack buttons signal `inspection pending` and `staff-confirmed ready`; they do not measure wash quality. Hub stores bounded local event history, shows stale status, and optionally forwards aggregate events. Cloud/app are never needed to count or display locally. No personal data or audio/image is collected.

## Electrical outline

USB-C 5 V SELV powers hub and chute; separate 3.3 V regulators feed modules. Chute sensor supply is 5 V only if its open-collector output is pulled up to 3.3 V; verify sensor datasheet. No line voltage, water-valve, heater, or dishwasher interface. Rack uses two AA cells and a low-quiescent 3.3 V regulator; no charging circuit. Provide local decoupling at every module and accessible reset/programming pads. Use keyed connectors and label polarity. Keep all electronics outside splash and food-contact zones.

## Failure handling

| Failure | Behavior |
|---|---|
| Chute sensor stuck/absent | Hub reports fault/stale; no inferred count |
| Hub reboot | Restore bounded counter/event log if nonvolatile writes are enabled; otherwise show unknown pending staff reconciliation |
| BLE loss | Nodes queue bounded events; hub marks node stale |
| Internet/broker loss | Local function continues, bounded queue with overflow health bit |
| Clock invalid | Sequence-based duplicate suppression; timestamp marked untrusted |
| Low battery | Rack health warning; readiness remains last known but becomes stale |
| Wash protocol uncertainty | Human inspection governs; device never asserts sanitization |

Queue maximum: 256 events per node; backoff 1,2,4…300 seconds with jitter. Sequence counters wrap modulo 65536. Durable storage and authenticated OTA are not implemented in this prototype.

## Build stages

1. Choose exact dev modules and confirm package pinouts / RF approvals.
2. Validate sensor output voltage and chute mechanical geometry; count calibration with representative cup shapes.
3. Implement HAL bindings and BLE Secure Connections commissioning; add broker authentication and API authorization before deployment.
4. Bench-test missed/double counts, duplicate frames, battery depletion, reboot, outages, and full queue.
5. Only after review, design PCB/enclosure, run ERC/DRC, ESD/EMC and wet-environment testing, and obtain venue food-safety review.

No certification, deployment suitability, or build-ready status is claimed.

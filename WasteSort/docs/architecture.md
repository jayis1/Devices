# WasteSort Architecture

## End-to-end data flow

1. Countertop Sorter scans an item and emits `sort_event`.
2. Hub applies municipality rule pack and publishes recommendation.
3. Bin Dock logs mass delta, lid-open duration, and fill-level trend.
4. Outdoor Pickup Beacon tracks curb and truck-empty events.
5. FastAPI backend stores telemetry in PostgreSQL and fans out WebSocket updates.
6. ML jobs consume event history for retraining and forecast generation.
7. Mobile app shows live stream state, contamination warnings, and pickup readiness.

## Edge/cloud split

### Edge (home gateway)
- Rule resolution
- Caching of barcodes and item embeddings
- Low-latency guidance for item disposal
- Temporary buffering during internet outage
- Local OTA manifest distribution

### Cloud
- Multi-home model training
- Municipal rule ingestion
- Historical dashboards and exports
- Push notification scheduling
- Fleet health analytics for deployed nodes

## Topic map

- `wastesort/hub/state`
- `wastesort/sorter/event`
- `wastesort/bin/<stream>/telemetry`
- `wastesort/beacon/pickup`
- `wastesort/alert/overflow`
- `wastesort/alert/missed-pickup`
- `wastesort/command/<node_id>`

## Database entities

- households
- bins
- sort_events
- bin_telemetry
- pickup_events
- recommendations
- rule_packs
- model_versions

## OTA flow

1. Backend publishes signed manifest.
2. Hub downloads and verifies SHA-256.
3. Hub schedules per-node delivery in low-traffic slots.
4. Node stages image, acknowledges chunk hashes, reboots into new slot.
5. Hub records success/failure and can roll back to previous version.

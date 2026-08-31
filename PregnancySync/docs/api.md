# PregnancySync API

## Endpoints

- `GET /api/v1/health` — service health and database path
- `GET /api/v1/nodes` — registered nodes by type
- `GET /api/v1/overview` — current household summary
- `POST /api/v1/telemetry/band` — belly band session ingest
- `POST /api/v1/telemetry/cuff` — BP cuff session ingest
- `POST /api/v1/telemetry/strip` — strip reader ingest
- `POST /api/v1/telemetry/pad` — sleep pad summary ingest
- `GET /api/v1/alerts` — generated alert list
- `POST /api/v1/actions/checkin` — record user symptom check-in

## Overview schema

`overview` returns:

- `reduced_movement_risk`
- `hypertensive_risk`
- `supine_sleep_risk`
- `hydration_status`
- `alerts[]`
- `recommended_actions[]`

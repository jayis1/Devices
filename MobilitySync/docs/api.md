# MobilitySync API

## REST endpoints

- `GET /health` -> service health
- `POST /telemetry/walker` -> ingest walker state and compute runaway risk
- `POST /telemetry/transfer` -> ingest transfer features and compute safety cue
- `POST /telemetry/doorway` -> ingest door approach and compute recommended action
- `POST /telemetry/band` -> ingest physiology and compute fatigue band
- `GET /commands` -> pending command history
- `GET /summary` -> independence and assistance summary

## MQTT topics

- `mobilitysync/hub/events`
- `mobilitysync/walker/<id>/telemetry`
- `mobilitysync/mat/<id>/telemetry`
- `mobilitysync/doorway/<id>/telemetry`
- `mobilitysync/band/<id>/telemetry`
- `mobilitysync/commands/<target>`

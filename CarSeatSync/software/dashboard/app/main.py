from __future__ import annotations

from fastapi import FastAPI, HTTPException

from .ml_inference import recommend, summarize_risk
from .models import EventStore, RecommendationResponse, TelemetryEvent

app = FastAPI(title='CarSeatSync Dashboard', version='0.1.0')
store = EventStore()


@app.get('/health')
def health() -> dict:
    return {'status': 'ok', 'service': 'carseatsync-dashboard'}


@app.post('/telemetry')
def ingest(event: TelemetryEvent) -> dict:
    saved = store.add(event)
    return {'accepted': True, 'child_id': saved.child_id, 'node': saved.node}


@app.get('/risk/summary')
def risk_summary(child_id: str) -> dict:
    events = store.list_for_child(child_id)
    if not events:
        raise HTTPException(status_code=404, detail='child not found')
    result = summarize_risk(events)
    result['child_id'] = child_id
    return result


@app.get('/children/{child_id}/timeline')
def timeline(child_id: str) -> list[dict]:
    events = store.list_for_child(child_id)
    if not events:
        raise HTTPException(status_code=404, detail='child not found')
    return [event.model_dump(mode='json') for event in events]


@app.get('/recommendations', response_model=RecommendationResponse)
def recommendations(child_id: str) -> RecommendationResponse:
    events = store.list_for_child(child_id)
    if not events:
        raise HTTPException(status_code=404, detail='child not found')
    return RecommendationResponse(**recommend(child_id, events))

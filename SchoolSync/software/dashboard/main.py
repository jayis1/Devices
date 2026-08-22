from __future__ import annotations

import asyncio
import json
import os
from datetime import datetime, timezone
from typing import Optional

import aiomqtt
import asyncpg
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import lunch_safety, overview, readiness_score, route_anomaly
from models import (LunchSafetyRequest, LunchSafetyResponse, ReadinessRequest,
                    ReadinessResponse, RouteAnomalyRequest, RouteAnomalyResponse,
                    RoutineEventIn)

app = FastAPI(title='SchoolSync API', version='1.0.0')
app.add_middleware(CORSMiddleware, allow_origins=['*'], allow_methods=['*'], allow_headers=['*'])

MQTT_BROKER = os.getenv('MQTT_BROKER', 'broker.schoolsync.local')
MQTT_PORT = int(os.getenv('MQTT_PORT', '1883'))
DATABASE_URL = os.getenv('DATABASE_URL', 'postgresql://schoolsync:***@localhost/schoolsync')

_db_pool: Optional[asyncpg.Pool] = None
_websockets: set[WebSocket] = set()


async def broadcast(event: dict) -> None:
    stale: list[WebSocket] = []
    for ws in list(_websockets):
        try:
            await ws.send_json(event)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        _websockets.discard(ws)


@app.on_event('startup')
async def startup() -> None:
    global _db_pool
    _db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=1, max_size=4)
    async with _db_pool.acquire() as conn:
        await conn.execute('''
            CREATE TABLE IF NOT EXISTS routine_events (
                id SERIAL PRIMARY KEY,
                child_id TEXT NOT NULL,
                event_type TEXT NOT NULL,
                ts TIMESTAMPTZ NOT NULL,
                details JSONB NOT NULL DEFAULT '{}'::jsonb
            );
        ''')
    asyncio.create_task(mqtt_loop())


@app.on_event('shutdown')
async def shutdown() -> None:
    if _db_pool is not None:
        await _db_pool.close()


async def mqtt_loop() -> None:
    try:
        async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
            await client.subscribe('schoolsync/#')
            async for message in client.messages:
                try:
                    payload = json.loads(message.payload.decode())
                except Exception:
                    continue
                await broadcast({'type': str(message.topic), 'payload': payload})
    except Exception as exc:  # pragma: no cover
        print(f'[mqtt] loop ended: {exc}')


@app.get('/api/v1/health')
async def health() -> dict:
    return {'status': 'ok', 'time': datetime.now(timezone.utc).isoformat()}


@app.get('/api/v1/overview')
async def get_overview() -> dict:
    sample = [
        {'child_id': 'ava', 'readiness_score': 86, 'status': 'ready'},
        {'child_id': 'leo', 'readiness_score': 68, 'status': 'missing_lunch'},
    ]
    return overview(sample)


@app.post('/api/v1/events/routine')
async def post_routine_event(event: RoutineEventIn) -> dict:
    if _db_pool is None:
        raise HTTPException(status_code=500, detail='database unavailable')
    async with _db_pool.acquire() as conn:
        await conn.execute(
            'INSERT INTO routine_events (child_id, event_type, ts, details) VALUES ($1,$2,$3,$4)',
            event.child_id,
            event.event_type,
            event.timestamp,
            json.dumps(event.details),
        )
    await broadcast({'type': 'routine_event', 'payload': event.model_dump(mode='json')})
    return {'accepted': True}


@app.post('/api/v1/predict/readiness', response_model=ReadinessResponse)
async def predict_readiness(payload: ReadinessRequest) -> ReadinessResponse:
    return ReadinessResponse(**readiness_score(payload.model_dump()))


@app.post('/api/v1/predict/lunch-safety', response_model=LunchSafetyResponse)
async def predict_lunch(payload: LunchSafetyRequest) -> LunchSafetyResponse:
    return LunchSafetyResponse(**lunch_safety(payload.model_dump()))


@app.post('/api/v1/predict/route-anomaly', response_model=RouteAnomalyResponse)
async def predict_route(payload: RouteAnomalyRequest) -> RouteAnomalyResponse:
    return RouteAnomalyResponse(**route_anomaly(payload.model_dump()))


@app.get('/api/v1/children/{child_id}/timeline')
async def child_timeline(child_id: str) -> list[dict]:
    if _db_pool is None:
        raise HTTPException(status_code=500, detail='database unavailable')
    async with _db_pool.acquire() as conn:
        rows = await conn.fetch('SELECT child_id, event_type, ts, details FROM routine_events WHERE child_id=$1 ORDER BY ts DESC LIMIT 50', child_id)
    return [dict(row) for row in rows]


@app.websocket('/ws/live')
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    _websockets.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _websockets.discard(websocket)

"""WasteSort FastAPI backend."""

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

from ml_inference import diversion_score, pickup_forecast, recommendations, resolve_sort
from models import BinTelemetry, PickupForecast, Recommendation, SortResolveRequest, SortResolveResponse

app = FastAPI(title="WasteSort API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.wastesort.local")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://wastesort:wastesort@localhost/wastesort")

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


@app.on_event("startup")
async def startup() -> None:
    global _db_pool
    _db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=1, max_size=5)
    async with _db_pool.acquire() as conn:
        await conn.execute(
            """
            CREATE TABLE IF NOT EXISTS bin_telemetry (
                id SERIAL PRIMARY KEY,
                bin_id TEXT NOT NULL,
                stream TEXT NOT NULL,
                fill_pct INT NOT NULL,
                mass_grams INT NOT NULL,
                voc_index INT NOT NULL,
                temp_c REAL NOT NULL,
                humidity_pct REAL NOT NULL,
                battery_mv INT NOT NULL,
                updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
            CREATE TABLE IF NOT EXISTS sort_events (
                id SERIAL PRIMARY KEY,
                barcode TEXT,
                municipality TEXT NOT NULL,
                stream TEXT NOT NULL,
                material TEXT NOT NULL,
                confidence REAL NOT NULL,
                created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
            CREATE TABLE IF NOT EXISTS pickup_events (
                id SERIAL PRIMARY KEY,
                event_type TEXT NOT NULL,
                curb_placed BOOL NOT NULL,
                created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
            """
        )
    asyncio.create_task(mqtt_loop())


@app.on_event("shutdown")
async def shutdown() -> None:
    if _db_pool is not None:
        await _db_pool.close()


async def mqtt_loop() -> None:
    try:
        async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
            await client.subscribe("wastesort/#")
            async for message in client.messages:
                topic = str(message.topic)
                try:
                    payload = json.loads(message.payload.decode())
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                await route_mqtt(topic, payload)
    except Exception as exc:  # pragma: no cover - environment dependent
        print(f"[mqtt] loop ended: {exc}")


async def route_mqtt(topic: str, payload: dict) -> None:
    if topic.startswith("wastesort/bin/"):
        telemetry = BinTelemetry(**payload)
        if _db_pool is not None:
            async with _db_pool.acquire() as conn:
                await conn.execute(
                    """
                    INSERT INTO bin_telemetry (bin_id, stream, fill_pct, mass_grams, voc_index, temp_c, humidity_pct, battery_mv)
                    VALUES ($1,$2,$3,$4,$5,$6,$7,$8)
                    """,
                    telemetry.bin_id,
                    telemetry.stream,
                    telemetry.fill_pct,
                    telemetry.mass_grams,
                    telemetry.voc_index,
                    telemetry.temp_c,
                    telemetry.humidity_pct,
                    telemetry.battery_mv,
                )
        await broadcast({"type": "bin_update", "payload": telemetry.model_dump(mode="json")})
    elif topic == "wastesort/beacon/pickup":
        await broadcast({"type": "pickup_update", "payload": payload})


@app.get("/api/v1/health")
async def health() -> dict:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat()}


@app.get("/api/v1/overview")
async def overview() -> dict:
    if _db_pool is None:
        raise HTTPException(status_code=500, detail="database unavailable")
    async with _db_pool.acquire() as conn:
        rows = await conn.fetch(
            """
            SELECT DISTINCT ON (bin_id) bin_id, stream, fill_pct, mass_grams, voc_index, temp_c, humidity_pct, battery_mv, updated_at
            FROM bin_telemetry
            ORDER BY bin_id, updated_at DESC
            """
        )
    bins = [dict(row) for row in rows]
    return {
        "diversion_score": diversion_score(bins),
        "bins": bins,
        "recommendations": recommendations(bins),
    }


@app.get("/api/v1/bins")
async def bins() -> list[dict]:
    if _db_pool is None:
        raise HTTPException(status_code=500, detail="database unavailable")
    async with _db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT bin_id, stream, fill_pct, mass_grams, voc_index, temp_c, humidity_pct, battery_mv, updated_at FROM bin_telemetry ORDER BY updated_at DESC LIMIT 20"
        )
    return [dict(row) for row in rows]


@app.post("/api/v1/sort/resolve", response_model=SortResolveResponse)
async def sort_resolve(request: SortResolveRequest) -> SortResolveResponse:
    result = resolve_sort(request.barcode, request.rgb_features, request.spectral, request.municipality)
    if _db_pool is not None:
        async with _db_pool.acquire() as conn:
            await conn.execute(
                "INSERT INTO sort_events (barcode, municipality, stream, material, confidence) VALUES ($1,$2,$3,$4,$5)",
                request.barcode,
                request.municipality,
                result["stream"],
                result["material"],
                result["confidence"],
            )
    return SortResolveResponse(**result)


@app.get("/api/v1/pickups/forecast", response_model=PickupForecast)
async def pickups_forecast(current_fill_pct: int = 72, curb_placed: bool = False) -> PickupForecast:
    return PickupForecast(**pickup_forecast(current_fill_pct=current_fill_pct, curb_placed=curb_placed))


@app.get("/api/v1/recommendations", response_model=list[Recommendation])
async def get_recommendations() -> list[Recommendation]:
    sample_bins = [
        {"stream": "recycle", "fill_pct": 88, "mass_grams": 5200, "voc_index": 70},
        {"stream": "compost", "fill_pct": 61, "mass_grams": 3800, "voc_index": 171},
    ]
    return [Recommendation(**item) for item in recommendations(sample_bins)]


@app.websocket("/ws/live")
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    _websockets.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _websockets.discard(websocket)

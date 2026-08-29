"""OutageSync FastAPI backend."""

from __future__ import annotations

import asyncio
import json
import os
import sqlite3
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import cold_summary, load_decisions, outage_forecast
from models import ColdTelemetry, FuelTelemetry, LoadDecision, OutletTelemetry, OutageForecast, PanelTelemetry, SystemOverview

try:
    import paho.mqtt.client as mqtt  # type: ignore
except Exception:  # pragma: no cover
    mqtt = None

app = FastAPI(title='OutageSync API', version='1.0.0')
app.add_middleware(CORSMiddleware, allow_origins=['*'], allow_methods=['*'], allow_headers=['*'])

DB_PATH = Path(os.getenv('OUTAGESYNC_DB', Path(__file__).with_name('outagesync.db')))
MQTT_BROKER = os.getenv('MQTT_BROKER', 'localhost')
MQTT_PORT = int(os.getenv('MQTT_PORT', '1883'))
_websockets: set[WebSocket] = set()
_mqtt_client: Optional[object] = None


def db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(db()) as conn:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS panel_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, vrms REAL, freq_hz REAL, thd_pct REAL,
                grid_present INTEGER, battery_soc REAL, reserve_minutes INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS cold_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, zone TEXT, product_temp_c REAL, ambient_temp_c REAL,
                door_open_seconds INTEGER, hold_minutes_remaining INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS outlet_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, label TEXT, watts REAL, priority INTEGER,
                relay_enabled INTEGER, medical INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS fuel_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, fuel_level_pct REAL, co2_ppm INTEGER, enclosure_temp_c REAL,
                safe_to_start INTEGER, runtime_minutes INTEGER, ts TEXT
            );
            """
        )
        conn.commit()


async def broadcast(event: dict) -> None:
    stale = []
    for ws in list(_websockets):
        try:
            await ws.send_json(event)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        _websockets.discard(ws)


def seed_if_empty() -> None:
    with closing(db()) as conn:
        count = conn.execute('SELECT COUNT(*) FROM panel_telemetry').fetchone()[0]
        if count:
            return
        now = datetime.now(timezone.utc).isoformat()
        conn.execute(
            'INSERT INTO panel_telemetry (node_id, vrms, freq_hz, thd_pct, grid_present, battery_soc, reserve_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('panel-1', 211.4, 59.72, 5.9, 0, 54.0, 118, now),
        )
        conn.executemany(
            'INSERT INTO cold_telemetry (node_id, zone, product_temp_c, ambient_temp_c, door_open_seconds, hold_minutes_remaining, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            [
                ('cold-1', 'fridge', 4.6, 23.1, 62, 198, now),
                ('cold-2', 'freezer', -16.8, 24.4, 18, 1320, now),
                ('cold-3', 'medicine', 5.2, 22.0, 0, 420, now),
            ],
        )
        conn.executemany(
            'INSERT INTO outlet_telemetry (node_id, label, watts, priority, relay_enabled, medical, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            [
                ('outlet-1', 'Router', 18, 1, 1, 0, now),
                ('outlet-2', 'Kitchen Fridge', 142, 1, 1, 0, now),
                ('outlet-3', 'TV Console', 126, 4, 1, 0, now),
                ('outlet-4', 'CPAP', 43, 0, 1, 1, now),
            ],
        )
        conn.execute(
            'INSERT INTO fuel_telemetry (node_id, fuel_level_pct, co2_ppm, enclosure_temp_c, safe_to_start, runtime_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            ('fuel-1', 68.0, 720, 29.2, 1, 44, now),
        )
        conn.commit()


@app.on_event('startup')
async def startup() -> None:
    init_db()
    seed_if_empty()
    if mqtt is not None:
        asyncio.create_task(start_mqtt())


async def start_mqtt() -> None:
    global _mqtt_client
    if mqtt is None:
        return
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(client, userdata, flags, reason_code, properties):  # pragma: no cover
        client.subscribe('outagesync/#')

    def on_message(client, userdata, msg):  # pragma: no cover
        try:
            payload = json.loads(msg.payload.decode())
        except Exception:
            return
        topic = msg.topic
        if topic.endswith('/panel'):
            ingest_panel(PanelTelemetry(**payload))
        elif topic.endswith('/cold'):
            ingest_cold(ColdTelemetry(**payload))
        elif topic.endswith('/outlet'):
            ingest_outlet(OutletTelemetry(**payload))
        elif topic.endswith('/fuel'):
            ingest_fuel(FuelTelemetry(**payload))

    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        _mqtt_client = client
    except Exception:
        _mqtt_client = None


def ingest_panel(payload: PanelTelemetry) -> None:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO panel_telemetry (node_id, vrms, freq_hz, thd_pct, grid_present, battery_soc, reserve_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.vrms, payload.freq_hz, payload.thd_pct, int(payload.grid_present), payload.battery_soc, payload.reserve_minutes, payload.ts.isoformat()),
        )
        conn.commit()


def ingest_cold(payload: ColdTelemetry) -> None:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO cold_telemetry (node_id, zone, product_temp_c, ambient_temp_c, door_open_seconds, hold_minutes_remaining, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.zone, payload.product_temp_c, payload.ambient_temp_c, payload.door_open_seconds, payload.hold_minutes_remaining, payload.ts.isoformat()),
        )
        conn.commit()


def ingest_outlet(payload: OutletTelemetry) -> None:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO outlet_telemetry (node_id, label, watts, priority, relay_enabled, medical, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.label, payload.watts, payload.priority, int(payload.relay_enabled), int(payload.medical), payload.ts.isoformat()),
        )
        conn.commit()


def ingest_fuel(payload: FuelTelemetry) -> None:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO fuel_telemetry (node_id, fuel_level_pct, co2_ppm, enclosure_temp_c, safe_to_start, runtime_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.fuel_level_pct, payload.co2_ppm, payload.enclosure_temp_c, int(payload.safe_to_start), payload.runtime_minutes, payload.ts.isoformat()),
        )
        conn.commit()


def latest_rows(table: str) -> list[dict]:
    with closing(db()) as conn:
        rows = conn.execute(f'SELECT * FROM {table} ORDER BY id DESC LIMIT 10').fetchall()
    return [dict(row) for row in rows]


@app.get('/api/v1/health')
async def health() -> dict:
    return {'status': 'ok', 'time': datetime.now(timezone.utc).isoformat(), 'db': str(DB_PATH)}


@app.get('/api/v1/overview', response_model=SystemOverview)
async def overview() -> SystemOverview:
    panel = latest_rows('panel_telemetry')[0]
    cold_nodes = latest_rows('cold_telemetry')
    outlets = latest_rows('outlet_telemetry')
    forecast = outage_forecast(panel)
    decisions = load_decisions(outlets, panel['reserve_minutes'])
    summary = cold_summary(cold_nodes)
    return SystemOverview(
        forecast=OutageForecast(**forecast),
        decisions=[LoadDecision(**d) for d in decisions],
        summary=summary,
    )


@app.get('/api/v1/panel')
async def panel() -> list[dict]:
    return latest_rows('panel_telemetry')


@app.get('/api/v1/cold-chain')
async def cold_chain() -> list[dict]:
    return latest_rows('cold_telemetry')


@app.get('/api/v1/outlets')
async def outlets() -> list[dict]:
    return latest_rows('outlet_telemetry')


@app.get('/api/v1/fuel')
async def fuel() -> list[dict]:
    return latest_rows('fuel_telemetry')


@app.websocket('/ws/live')
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    _websockets.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _websockets.discard(websocket)

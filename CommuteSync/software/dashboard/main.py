from __future__ import annotations

import os
import sqlite3
from contextlib import asynccontextmanager, closing
from datetime import datetime, timezone
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import exposure_risk, lateness_risk, readiness_risk, recommended_actions, theft_risk
from models import BagTelemetry, CheckIn, DeskTelemetry, EntryTelemetry, MobilityTelemetry, Overview, RiskSummary

DB_PATH = Path(os.getenv('COMMUTESYNC_DB', Path(__file__).with_name('commutesync.db')))


@asynccontextmanager
async def lifespan(_: FastAPI):
    init_db()
    seed_if_empty()
    yield


app = FastAPI(title='CommuteSync API', version='1.0.0', lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=['*'], allow_methods=['*'], allow_headers=['*'])


def db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(db()) as conn:
        conn.executescript(
            '''
            CREATE TABLE IF NOT EXISTS entry_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, required_items INTEGER, confirmed_items INTEGER,
                bag_present INTEGER, badge_seen INTEGER, keys_seen INTEGER,
                departure_in_minutes INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS bag_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, tamper_score REAL, separation_m REAL,
                motion_state TEXT, battery_mv INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS mobility_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, route_minutes INTEGER, eta_delta_minutes INTEGER,
                pm25_ug_m3 REAL, voc_index INTEGER, vibration_rms REAL,
                crash_flag INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS desk_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, arrival_confirmed INTEGER, items_left_behind INTEGER,
                bag_present INTEGER, laptop_present INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS checkins (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event TEXT, note TEXT, ts TEXT
            );
            '''
        )
        conn.commit()


def seed_if_empty() -> None:
    with closing(db()) as conn:
        existing = conn.execute('SELECT COUNT(*) FROM entry_telemetry').fetchone()[0]
        if existing:
            return
        now = datetime.now(timezone.utc).isoformat()
        conn.execute(
            'INSERT INTO entry_telemetry (node_id, required_items, confirmed_items, bag_present, badge_seen, keys_seen, departure_in_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('entry-1', 5, 4, 1, 0, 1, 12, now),
        )
        conn.execute(
            'INSERT INTO bag_telemetry (node_id, tamper_score, separation_m, motion_state, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?)',
            ('bag-1', 0.18, 0.6, 'idle', 2920, now),
        )
        conn.execute(
            'INSERT INTO mobility_telemetry (node_id, route_minutes, eta_delta_minutes, pm25_ug_m3, voc_index, vibration_rms, crash_flag, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('mobility-1', 33, 7, 24.5, 118, 0.82, 0, now),
        )
        conn.execute(
            'INSERT INTO desk_telemetry (node_id, arrival_confirmed, items_left_behind, bag_present, laptop_present, ts) VALUES (?, ?, ?, ?, ?, ?)',
            ('desk-1', 1, 0, 1, 1, now),
        )
        conn.commit()


def latest_rows(table: str, limit: int = 10) -> list[dict]:
    with closing(db()) as conn:
        rows = conn.execute(f'SELECT * FROM {table} ORDER BY id DESC LIMIT ?', (limit,)).fetchall()
    return [dict(row) for row in rows]


@app.get('/api/v1/health')
def health() -> dict:
    return {'status': 'ok', 'time': datetime.now(timezone.utc).isoformat(), 'db': str(DB_PATH)}


@app.get('/api/v1/nodes')
def nodes() -> dict:
    return {
        'entry_nodes': sorted({row['node_id'] for row in latest_rows('entry_telemetry')}),
        'bag_nodes': sorted({row['node_id'] for row in latest_rows('bag_telemetry')}),
        'mobility_nodes': sorted({row['node_id'] for row in latest_rows('mobility_telemetry')}),
        'desk_nodes': sorted({row['node_id'] for row in latest_rows('desk_telemetry')}),
    }


@app.get('/api/v1/overview', response_model=Overview)
def overview() -> Overview:
    entry_rows = latest_rows('entry_telemetry')
    bag_rows = latest_rows('bag_telemetry')
    mobility_rows = latest_rows('mobility_telemetry')
    desk_rows = latest_rows('desk_telemetry')
    readiness = readiness_risk(entry_rows, desk_rows)
    lateness = lateness_risk(entry_rows, mobility_rows)
    exposure = exposure_risk(mobility_rows)
    theft = theft_risk(bag_rows, mobility_rows)
    actions = recommended_actions(readiness, lateness, exposure, theft, entry_rows)
    return Overview(
        readiness_risk=RiskSummary(**readiness),
        lateness_risk=RiskSummary(**lateness),
        exposure_risk=RiskSummary(**exposure),
        theft_risk=RiskSummary(**theft),
        recommended_actions=actions,
    )


@app.post('/api/v1/telemetry/entry')
def ingest_entry(payload: EntryTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO entry_telemetry (node_id, required_items, confirmed_items, bag_present, badge_seen, keys_seen, departure_in_minutes, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.required_items, payload.confirmed_items, int(payload.bag_present), int(payload.badge_seen), int(payload.keys_seen), payload.departure_in_minutes, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/bag')
def ingest_bag(payload: BagTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO bag_telemetry (node_id, tamper_score, separation_m, motion_state, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.tamper_score, payload.separation_m, payload.motion_state, payload.battery_mv, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/mobility')
def ingest_mobility(payload: MobilityTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO mobility_telemetry (node_id, route_minutes, eta_delta_minutes, pm25_ug_m3, voc_index, vibration_rms, crash_flag, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.route_minutes, payload.eta_delta_minutes, payload.pm25_ug_m3, payload.voc_index, payload.vibration_rms, int(payload.crash_flag), payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/desk')
def ingest_desk(payload: DeskTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO desk_telemetry (node_id, arrival_confirmed, items_left_behind, bag_present, laptop_present, ts) VALUES (?, ?, ?, ?, ?, ?)',
            (payload.node_id, int(payload.arrival_confirmed), payload.items_left_behind, int(payload.bag_present), int(payload.laptop_present), payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/actions/checkin')
def ingest_checkin(payload: CheckIn) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO checkins (event, note, ts) VALUES (?, ?, ?)',
            (payload.event, payload.note, datetime.now(timezone.utc).isoformat()),
        )
        conn.commit()
    return {'recorded': True, 'event': payload.event}

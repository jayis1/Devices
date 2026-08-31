from __future__ import annotations

import os
import sqlite3
from contextlib import asynccontextmanager, closing
from datetime import datetime, timezone
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import (
    derive_alerts,
    hydration_status,
    hypertensive_risk,
    recommended_actions,
    reduced_movement_risk,
    supine_sleep_risk,
)
from models import BandTelemetry, CheckIn, CuffTelemetry, Overview, PadTelemetry, RiskSummary, StripTelemetry

DB_PATH = Path(os.getenv('PREGNANCYSYNC_DB', Path(__file__).with_name('pregnancysync.db')))


@asynccontextmanager
async def lifespan(_: FastAPI):
    init_db()
    seed_if_empty()
    yield


app = FastAPI(title='PregnancySync API', version='1.0.0', lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=['*'], allow_methods=['*'], allow_headers=['*'])


def db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(db()) as conn:
        conn.executescript(
            '''
            CREATE TABLE IF NOT EXISTS band_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, movement_count_10m INTEGER, movement_variability REAL,
                posture_pct_left REAL, posture_pct_supine REAL, skin_temp_c REAL,
                ehg_activity_index REAL, battery_mv INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS cuff_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, systolic_mmHg INTEGER, diastolic_mmHg INTEGER,
                map_mmHg INTEGER, pulse_rate_bpm INTEGER, waveform_quality REAL,
                motion_artifact_score REAL, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS strip_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, protein_level TEXT, glucose_level TEXT, ketone_level TEXT,
                specific_gravity REAL, nitrite_positive INTEGER, hydration_bottle_ml INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS pad_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, hours_recorded REAL, supine_minutes INTEGER,
                left_side_minutes INTEGER, respiration_rate REAL, restlessness_index REAL, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS checkins (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                symptom TEXT, severity INTEGER, note TEXT, ts TEXT
            );
            '''
        )
        conn.commit()


def seed_if_empty() -> None:
    with closing(db()) as conn:
        existing = conn.execute('SELECT COUNT(*) FROM band_telemetry').fetchone()[0]
        if existing:
            return
        now = datetime.now(timezone.utc).isoformat()
        band_rows = [
            ('band-1', 17, 0.81, 62.0, 12.0, 34.2, 6.1, 3950, now),
            ('band-1', 14, 0.72, 58.0, 18.0, 34.4, 8.0, 3920, now),
            ('band-1', 11, 0.60, 51.0, 28.0, 34.5, 10.4, 3890, now),
        ]
        conn.executemany(
            'INSERT INTO band_telemetry (node_id, movement_count_10m, movement_variability, posture_pct_left, posture_pct_supine, skin_temp_c, ehg_activity_index, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)',
            band_rows,
        )
        conn.execute(
            'INSERT INTO cuff_telemetry (node_id, systolic_mmHg, diastolic_mmHg, map_mmHg, pulse_rate_bpm, waveform_quality, motion_artifact_score, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('cuff-1', 138, 89, 105, 82, 0.94, 0.08, now),
        )
        conn.execute(
            'INSERT INTO strip_telemetry (node_id, protein_level, glucose_level, ketone_level, specific_gravity, nitrite_positive, hydration_bottle_ml, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('strip-1', 'trace', 'negative', 'trace', 1.024, 0, 1450, now),
        )
        conn.execute(
            'INSERT INTO pad_telemetry (node_id, hours_recorded, supine_minutes, left_side_minutes, respiration_rate, restlessness_index, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            ('pad-1', 7.4, 94, 248, 16.8, 2.4, now),
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
        'band_nodes': sorted({row['node_id'] for row in latest_rows('band_telemetry')}),
        'cuff_nodes': sorted({row['node_id'] for row in latest_rows('cuff_telemetry')}),
        'strip_nodes': sorted({row['node_id'] for row in latest_rows('strip_telemetry')}),
        'pad_nodes': sorted({row['node_id'] for row in latest_rows('pad_telemetry')}),
    }


@app.get('/api/v1/overview', response_model=Overview)
def overview() -> Overview:
    band_rows = latest_rows('band_telemetry')
    cuff_rows = latest_rows('cuff_telemetry')
    strip_rows = latest_rows('strip_telemetry')
    pad_rows = latest_rows('pad_telemetry')
    movement = reduced_movement_risk(band_rows)
    pressure = hypertensive_risk(cuff_rows, strip_rows, pad_rows)
    sleep = supine_sleep_risk(pad_rows)
    hydration = hydration_status(strip_rows)
    alerts = derive_alerts(movement, pressure, sleep, hydration)
    actions = recommended_actions(movement, pressure, sleep, hydration)
    return Overview(
        reduced_movement_risk=RiskSummary(**movement),
        hypertensive_risk=RiskSummary(**pressure),
        supine_sleep_risk=RiskSummary(**sleep),
        hydration_status=hydration,
        alerts=alerts,
        recommended_actions=actions,
    )


@app.get('/api/v1/alerts')
def alerts() -> dict:
    data = overview()
    return {'alerts': data.alerts, 'recommended_actions': data.recommended_actions}


@app.post('/api/v1/telemetry/band')
def ingest_band(payload: BandTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO band_telemetry (node_id, movement_count_10m, movement_variability, posture_pct_left, posture_pct_supine, skin_temp_c, ehg_activity_index, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.movement_count_10m, payload.movement_variability, payload.posture_pct_left, payload.posture_pct_supine, payload.skin_temp_c, payload.ehg_activity_index, payload.battery_mv, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/cuff')
def ingest_cuff(payload: CuffTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO cuff_telemetry (node_id, systolic_mmHg, diastolic_mmHg, map_mmHg, pulse_rate_bpm, waveform_quality, motion_artifact_score, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.systolic_mmHg, payload.diastolic_mmHg, payload.map_mmHg, payload.pulse_rate_bpm, payload.waveform_quality, payload.motion_artifact_score, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/strip')
def ingest_strip(payload: StripTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO strip_telemetry (node_id, protein_level, glucose_level, ketone_level, specific_gravity, nitrite_positive, hydration_bottle_ml, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.protein_level, payload.glucose_level, payload.ketone_level, payload.specific_gravity, int(payload.nitrite_positive), payload.hydration_bottle_ml, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/pad')
def ingest_pad(payload: PadTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO pad_telemetry (node_id, hours_recorded, supine_minutes, left_side_minutes, respiration_rate, restlessness_index, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.hours_recorded, payload.supine_minutes, payload.left_side_minutes, payload.respiration_rate, payload.restlessness_index, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/actions/checkin')
def ingest_checkin(payload: CheckIn) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO checkins (symptom, severity, note, ts) VALUES (?, ?, ?, ?)',
            (payload.symptom, payload.severity, payload.note, datetime.now(timezone.utc).isoformat()),
        )
        conn.commit()
    return {'recorded': True, 'symptom': payload.symptom}

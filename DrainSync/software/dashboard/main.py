from __future__ import annotations

import json
import os
import sqlite3
from contextlib import asynccontextmanager, closing
from datetime import datetime, timezone
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import backup_forecast, clog_risks, trap_recommendations, valve_state
from models import (
    ActuatorTelemetry,
    BackupForecast,
    FlowTelemetry,
    Overview,
    RiskItem,
    StackTelemetry,
    TrapRecommendation,
    TrapTelemetry,
    ValveCommand,
    ValveState,
)

try:
    import paho.mqtt.client as mqtt  # type: ignore
except Exception:  # pragma: no cover
    mqtt = None

DB_PATH = Path(os.getenv('DRAINSYNC_DB', Path(__file__).with_name('drainsync.db')))


@asynccontextmanager
async def lifespan(_: FastAPI):
    init_db()
    seed_if_empty()
    yield


app = FastAPI(title='DrainSync API', version='1.0.0', lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=['*'], allow_methods=['*'], allow_headers=['*'])


def db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with closing(db()) as conn:
        conn.executescript(
            '''
            CREATE TABLE IF NOT EXISTS flow_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, zone TEXT, branch_id TEXT, duration_ms INTEGER,
                turbulence REAL, vibration_rms REAL, gas_index REAL, leak INTEGER,
                temperature_c REAL, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS trap_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, zone TEXT, trap_depth_raw INTEGER, h2s_ppb REAL,
                humidity_rh REAL, primer_cycles INTEGER, water_present INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS stack_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, level_mm REAL, diff_pressure_pa REAL,
                surge_count INTEGER, battery_mv INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS actuator_telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT, position_pct REAL, target_pct REAL, motor_current_ma REAL,
                fault_bits INTEGER, healthy INTEGER, ts TEXT
            );
            CREATE TABLE IF NOT EXISTS commands (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                command TEXT, reason TEXT, ts TEXT
            );
            '''
        )
        conn.commit()


def seed_if_empty() -> None:
    with closing(db()) as conn:
        existing = conn.execute('SELECT COUNT(*) FROM flow_telemetry').fetchone()[0]
        if existing:
            return
        now = datetime.now(timezone.utc).isoformat()
        conn.executemany(
            'INSERT INTO flow_telemetry (node_id, zone, branch_id, duration_ms, turbulence, vibration_rms, gas_index, leak, temperature_c, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
            [
                ('sink-1', 'kitchen', 'kitchen-west', 2140, 241.0, 88.0, 42.0, 0, 44.0, now),
                ('sink-2', 'laundry', 'laundry', 980, 84.0, 51.0, 18.0, 0, 31.0, now),
                ('sink-3', 'bath', 'guest-bath', 1220, 130.0, 40.0, 16.0, 0, 28.0, now)
            ]
        )
        conn.execute(
            'INSERT INTO trap_telemetry (node_id, zone, trap_depth_raw, h2s_ppb, humidity_rh, primer_cycles, water_present, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            ('floor-1', 'basement', 250, 33.0, 61.2, 1, 0, now)
        )
        conn.execute(
            'INSERT INTO stack_telemetry (node_id, level_mm, diff_pressure_pa, surge_count, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?)',
            ('stack-1', 420.0, 64.0, 19, 7660, now)
        )
        conn.execute(
            'INSERT INTO actuator_telemetry (node_id, position_pct, target_pct, motor_current_ma, fault_bits, healthy, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            ('act-1', 0.0, 0.0, 120.0, 0, 1, now)
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
        'flow_nodes': [row['node_id'] for row in latest_rows('flow_telemetry')],
        'trap_nodes': [row['node_id'] for row in latest_rows('trap_telemetry')],
        'stack_nodes': [row['node_id'] for row in latest_rows('stack_telemetry')],
        'actuator_nodes': [row['node_id'] for row in latest_rows('actuator_telemetry')],
    }


@app.get('/api/v1/overview', response_model=Overview)
def overview() -> Overview:
    flow_rows = latest_rows('flow_telemetry')
    trap_rows = latest_rows('trap_telemetry')
    stack = latest_rows('stack_telemetry', 1)[0]
    actuator = latest_rows('actuator_telemetry', 1)[0]
    forecast = BackupForecast(**backup_forecast(stack, actuator))
    valve = ValveState(**valve_state(actuator))
    clog_items = [RiskItem(**row) for row in clog_risks(flow_rows)]
    trap_items = [TrapRecommendation(**row) for row in trap_recommendations(trap_rows)]
    return Overview(
        backup_forecast=forecast,
        valve=valve,
        top_clog_risks=clog_items,
        trap_recommendations=trap_items,
    )


@app.post('/api/v1/telemetry/flow')
def ingest_flow(payload: FlowTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO flow_telemetry (node_id, zone, branch_id, duration_ms, turbulence, vibration_rms, gas_index, leak, temperature_c, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.zone, payload.branch_id, payload.duration_ms, payload.turbulence, payload.vibration_rms, payload.gas_index, int(payload.leak), payload.temperature_c, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/trap')
def ingest_trap(payload: TrapTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO trap_telemetry (node_id, zone, trap_depth_raw, h2s_ppb, humidity_rh, primer_cycles, water_present, ts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.zone, payload.trap_depth_raw, payload.h2s_ppb, payload.humidity_rh, payload.primer_cycles, int(payload.water_present), payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/stack')
def ingest_stack(payload: StackTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO stack_telemetry (node_id, level_mm, diff_pressure_pa, surge_count, battery_mv, ts) VALUES (?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.level_mm, payload.diff_pressure_pa, payload.surge_count, payload.battery_mv, payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/telemetry/actuator')
def ingest_actuator(payload: ActuatorTelemetry) -> dict:
    with closing(db()) as conn:
        conn.execute(
            'INSERT INTO actuator_telemetry (node_id, position_pct, target_pct, motor_current_ma, fault_bits, healthy, ts) VALUES (?, ?, ?, ?, ?, ?, ?)',
            (payload.node_id, payload.position_pct, payload.target_pct, payload.motor_current_ma, payload.fault_bits, int(payload.healthy), payload.ts.isoformat()),
        )
        conn.commit()
    return {'stored': True, 'node_id': payload.node_id}


@app.post('/api/v1/commands/valve')
def command_valve(payload: ValveCommand) -> dict:
    with closing(db()) as conn:
        conn.execute('INSERT INTO commands (command, reason, ts) VALUES (?, ?, ?)', (payload.command, payload.reason, datetime.now(timezone.utc).isoformat()))
        conn.commit()
    return {'accepted': True, 'command': payload.command}

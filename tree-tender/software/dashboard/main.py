from datetime import datetime, timezone
from pydantic import BaseModel, Field

class Observation(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=40)
    seq: int = Field(ge=0, le=65535)
    ts: int = Field(ge=0)
    kind: str = Field(pattern='^(soil|weather)$')
    soil_pct: float | None = Field(default=None, ge=0, le=100)
    temp_c: float | None = Field(default=None, ge=-40, le=85)
    rh_pct: float | None = Field(default=None, ge=0, le=100)
    rain_mm: float | None = Field(default=None, ge=0)
    pressure_hpa: float | None = Field(default=None, ge=300, le=1200)

from fastapi import FastAPI, HTTPException
import sqlite3
import os

app = FastAPI(title='TreeTender API', version='0.1.0')
DB_PATH = os.getenv('DATABASE_PATH', 'treetender.db')
def db():
    connection = sqlite3.connect(DB_PATH)
    connection.row_factory = sqlite3.Row
    return connection
with db() as conn:
    conn.execute('CREATE TABLE IF NOT EXISTS observations (site TEXT, node TEXT, seq INTEGER, ts INTEGER, payload TEXT, PRIMARY KEY(site,node,seq,ts))')
    conn.execute('CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT, node TEXT, visited_at TEXT, note TEXT)')

@app.get('/health')
def health(): return {'status':'ok'}

@app.post('/sites/{site}/observations')
def ingest(site: str, item: Observation):
    if item.kind == 'soil' and item.soil_pct is None: raise HTTPException(422, 'soil_pct required for soil observation')
    if item.kind == 'weather' and item.soil_pct is not None: raise HTTPException(422, 'soil_pct not valid for weather observation')
    payload = item.model_dump_json()
    with db() as conn:
        conn.execute('INSERT OR IGNORE INTO observations(site,node,seq,ts,payload) VALUES(?,?,?,?,?)', (site,item.node,item.seq,item.ts,payload))
    return {'accepted': True}

@app.get('/sites/{site}/observations')
def observations(site: str, limit: int = 100):
    if not 1 <= limit <= 1000: raise HTTPException(422, 'limit must be 1..1000')
    with db() as conn:
        rows=[r['payload'] for r in conn.execute('SELECT payload FROM observations WHERE site=? ORDER BY ts DESC LIMIT ?', (site,limit)).fetchall()]
    return [Observation.model_validate_json(r).model_dump() for r in rows]

@app.post('/sites/{site}/nodes/{node}/visits')
def record_visit(site: str, node: str, note: str = ''):
    with db() as conn:
        conn.execute('INSERT INTO visits(site,node,visited_at,note) VALUES(?,?,?,?)', (site,node,datetime.now(timezone.utc).isoformat(),note[:500]))
    return {'recorded':True}

@app.get('/sites/{site}/visits')
def visits(site: str):
    with db() as conn:
        return [dict(r) for r in conn.execute('SELECT node,visited_at,note FROM visits WHERE site=? ORDER BY id DESC LIMIT 100', (site,)).fetchall()]

# MQTT forwarding is provided by the separately run mqtt_bridge.py adapter.

import math
import os
import sqlite3
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel, ConfigDict, Field

DB = os.getenv('SEEDSHARE_DB', 'seedshare.sqlite3')
app = FastAPI(title='SeedShare local event API', version='1.0')

class Event(BaseModel):
    model_config = ConfigDict(extra='forbid')
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=48)
    sequence: int = Field(ge=0, le=4294967295)
    kind: str = Field(min_length=1, max_length=32)
    ts_utc: str | None = None
    values: dict[str, float | str | bool | None]

def connect():
    con = sqlite3.connect(DB)
    con.execute('CREATE TABLE IF NOT EXISTS events (node TEXT, seq INTEGER, body TEXT, PRIMARY KEY(node,seq))')
    return con

@app.get('/health')
def health(): return {'ok': True}

@app.post('/api/v1/events')
def create_event(event: Event):
    for value in event.values.values():
        if isinstance(value, float) and not math.isfinite(value):
            raise HTTPException(422, 'non-finite numeric value')
    body = event.model_dump_json()
    with connect() as con:
        try: con.execute('INSERT INTO events VALUES (?,?,?)', (event.node_id, event.sequence, body))
        except sqlite3.IntegrityError: return {'accepted': True, 'duplicate': True}
    return {'accepted': True, 'duplicate': False}

@app.get('/api/v1/events')
def list_events(limit: int = Query(100, ge=1, le=500)):
    import json
    with connect() as con:
        rows = con.execute('SELECT body FROM events ORDER BY rowid DESC LIMIT ?', (limit,)).fetchall()
    return [json.loads(row[0]) for row in rows]

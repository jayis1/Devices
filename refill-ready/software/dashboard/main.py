from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3
from pathlib import Path

DB = Path(__file__).with_name('events.sqlite3')
app = FastAPI(title='RefillReady prototype API')

class Event(BaseModel):
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    kind: str = Field(pattern='^(fixture|path_check|health)$')
    value_a: int
    value_b: int
    received_at: str

def connect():
    db = sqlite3.connect(DB)
    db.execute('CREATE TABLE IF NOT EXISTS events (node_id INTEGER, sequence INTEGER, kind TEXT, value_a INTEGER, value_b INTEGER, received_at TEXT, PRIMARY KEY(node_id, sequence))')
    return db

@app.get('/health')
def health():
    return {'status': 'ok'}

@app.post('/api/v1/events')
def ingest(event: Event):
    db = connect()
    try:
        db.execute('INSERT INTO events VALUES (?, ?, ?, ?, ?, ?)', (event.node_id, event.sequence, event.kind, event.value_a, event.value_b, event.received_at))
        db.commit()
        return {'accepted': True, 'duplicate': False}
    except sqlite3.IntegrityError:
        return {'accepted': True, 'duplicate': True}
    finally:
        db.close()

@app.get('/api/v1/events')
def list_events(limit: int = 100):
    if not 1 <= limit <= 500:
        raise HTTPException(400, 'limit must be 1..500')
    db = connect()
    try:
        rows = db.execute('SELECT node_id, sequence, kind, value_a, value_b, received_at FROM events ORDER BY received_at DESC LIMIT ?', (limit,)).fetchall()
        return {'events': [dict(zip(('node_id','sequence','kind','value_a','value_b','received_at'), row)) for row in rows]}
    finally:
        db.close()

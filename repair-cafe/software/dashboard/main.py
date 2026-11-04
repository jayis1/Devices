"""Local demo API. No auth, BLE adapter, or MQTT integration: bind to localhost only."""
from datetime import datetime, timezone
import sqlite3
from pathlib import Path
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DB = Path(__file__).with_name("repair-cafe.sqlite3")
app = FastAPI(title="RepairCafe local demo", version="0.1.0")

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    event_type: int = Field(ge=1, le=3)
    node_id: int = Field(gt=0, le=255)
    sequence: int = Field(gt=0, le=0xffffffff)
    ticket_id: int = Field(default=0, ge=0, le=65535)
    state: int = Field(default=0, ge=0, le=3)

def connect():
    c=sqlite3.connect(DB)
    c.execute("CREATE TABLE IF NOT EXISTS events(node_id INTEGER, sequence INTEGER, payload TEXT, received TEXT, PRIMARY KEY(node_id,sequence))")
    return c

@app.post('/v1/events')
def ingest(event: Event):
    c=connect()
    try:
        c.execute("INSERT INTO events VALUES(?,?,?,?)",(event.node_id,event.sequence,event.model_dump_json(),datetime.now(timezone.utc).isoformat()))
        c.commit()
    except sqlite3.IntegrityError:
        raise HTTPException(status_code=409, detail='duplicate sequence')
    finally: c.close()
    return {'accepted': True}

@app.get('/v1/events')
def events(limit: int=100):
    if not 1 <= limit <= 500: raise HTTPException(status_code=422, detail='limit must be 1..500')
    c=connect()
    try: rows=c.execute("SELECT payload,received FROM events ORDER BY rowid DESC LIMIT ?",(limit,)).fetchall()
    finally: c.close()
    return [{'event':__import__('json').loads(p),'received_at':t} for p,t in rows]

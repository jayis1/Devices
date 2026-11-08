import os
import sqlite3
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="ClaimLoop local event API", version="0.1.0")
DB_PATH = os.environ.get("CLAIMLOOP_DB", "claimloop.sqlite3")

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=23, pattern=r"^[A-Za-z0-9_-]+$")
    sequence: int = Field(ge=0, le=4294967295)
    kind: str
    value: int = Field(ge=-30000, le=30000)
    monotonic_ms: int = Field(ge=0, le=4294967295)

@app.on_event("startup")
def initialize():
    with sqlite3.connect(DB_PATH) as db:
        db.execute("CREATE TABLE IF NOT EXISTS events(node TEXT, seq INTEGER, kind TEXT, value INTEGER, monotonic_ms INTEGER, received TEXT, PRIMARY KEY(node,seq))")

@app.post("/events", status_code=202)
def ingest(event: Event):
    limits = {"tray_weight_g": (-0, 30000), "shelf_door": (0, 1), "staff_intake": (0, 30000)}
    if event.kind not in limits or not limits[event.kind][0] <= event.value <= limits[event.kind][1]:
        raise HTTPException(422, "unsupported event kind or value")
    try:
        with sqlite3.connect(DB_PATH) as db:
            db.execute("INSERT INTO events VALUES(?,?,?,?,?,?)", (event.node_id, event.sequence, event.kind, event.value, event.monotonic_ms, datetime.now(timezone.utc).isoformat()))
    except sqlite3.IntegrityError:
        raise HTTPException(409, "duplicate event")
    return {"accepted": True}

@app.get("/events")
def recent():
    with sqlite3.connect(DB_PATH) as db:
        rows = db.execute("SELECT node,seq,kind,value,monotonic_ms,received FROM events ORDER BY received DESC LIMIT 100").fetchall()
    return {"events": rows}

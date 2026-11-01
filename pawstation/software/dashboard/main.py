from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3

app = FastAPI(title="PawStation local event intake")
DB = "pawstation.sqlite3"

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=32)
    seq: int = Field(ge=0)
    kind: str
    value: int
    unit: str
    utc_ms: int | None = None

@app.on_event("startup")
def initialize():
    with sqlite3.connect(DB) as db:
        db.execute("CREATE TABLE IF NOT EXISTS events(node TEXT, seq INTEGER, kind TEXT, value INTEGER, unit TEXT, utc_ms INTEGER, received TEXT, PRIMARY KEY(node,seq))")

@app.post("/events", status_code=202)
def ingest(event: Event):
    allowed = {"bag_level_pct", "bin_fill_pct", "lid_open", "service_confirmed"}
    if event.kind not in allowed or event.unit not in {"pct", "bool"}:
        raise HTTPException(422, "unsupported kind or unit")
    if event.unit == "pct" and not 0 <= event.value <= 100:
        raise HTTPException(422, "percent out of range")
    with sqlite3.connect(DB) as db:
        try:
            db.execute("INSERT INTO events VALUES(?,?,?,?,?,?,?)", (event.node_id,event.seq,event.kind,event.value,event.unit,event.utc_ms,datetime.now(timezone.utc).isoformat()))
        except sqlite3.IntegrityError:
            raise HTTPException(409, "duplicate event")
    return {"accepted": True}

@app.get("/events")
def recent():
    with sqlite3.connect(DB) as db:
        rows = db.execute("SELECT node,seq,kind,value,unit,utc_ms,received FROM events ORDER BY received DESC LIMIT 100").fetchall()
    return {"events": rows}

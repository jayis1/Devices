# FrostLine API demo
from datetime import datetime, timezone
import json
import os
import sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DB_PATH = os.environ.get("FROSTLINE_DB", "frostline.sqlite3")
app = FastAPI(title="FrostLine local API", version="0.1.0")

class Telemetry(BaseModel):
    site_id: str = Field(min_length=1, max_length=64)
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    observed_at: datetime
    surface_temp_c: float | None = Field(default=None, ge=-80, le=80)
    air_temp_c: float | None = Field(default=None, ge=-80, le=80)
    relative_humidity_pct: float | None = Field(default=None, ge=0, le=100)
    wetness: int | None = Field(default=None, ge=0, le=65535)
    battery_mv: int = Field(ge=0, le=10000)

def connect():
    db=sqlite3.connect(DB_PATH)
    db.execute("CREATE TABLE IF NOT EXISTS telemetry (site TEXT,node INTEGER,seq INTEGER,observed TEXT,payload TEXT,PRIMARY KEY(site,node,seq))")
    return db

@app.get("/health")
def health(): return {"status":"ok"}

@app.post("/api/v1/telemetry")
def ingest(data: Telemetry):
    with connect() as db:
        try: db.execute("INSERT INTO telemetry VALUES (?,?,?,?,?)", (data.site_id,data.node_id,data.sequence,data.observed_at.isoformat(),data.model_dump_json()))
        except sqlite3.IntegrityError: raise HTTPException(status_code=409, detail="duplicate node sequence")
    return {"accepted":True}

@app.get("/api/v1/sites/{site_id}/latest")
def latest(site_id: str):
    with connect() as db:
        rows=db.execute("SELECT payload FROM telemetry WHERE site=? ORDER BY observed DESC LIMIT 32",(site_id,)).fetchall()
    if not rows: raise HTTPException(status_code=404,detail="no telemetry")
    return {"site_id":site_id,"records":[json.loads(r[0]) for r in rows],"as_of":datetime.now(timezone.utc).isoformat()}

# Production MQTT TLS client and per-device authorization intentionally remain deployment work.

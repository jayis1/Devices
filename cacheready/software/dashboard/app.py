from datetime import datetime, timezone
from fastapi import FastAPI
from pydantic import BaseModel, Field
import sqlite3

DB = "cacheready.sqlite3"
app = FastAPI(title="CacheReady prototype API")

class Observation(BaseModel):
    node_id: int = Field(ge=0, le=255)
    sequence: int = Field(ge=0, le=255)
    observed_at: datetime
    temperature_c: float | None = Field(default=None, ge=-40, le=85)
    door_open: bool | None = None
    supply_v: float | None = Field(default=None, ge=0, le=30)

@app.on_event("startup")
def init_db():
    with sqlite3.connect(DB) as db:
        db.execute("CREATE TABLE IF NOT EXISTS observations (node_id INTEGER, sequence INTEGER, observed_at TEXT, temperature_c REAL, door_open INTEGER, supply_v REAL, UNIQUE(node_id, sequence))")

@app.get("/health")
def health():
    return {"status": "prototype", "utc": datetime.now(timezone.utc).isoformat()}

@app.post("/api/v1/observations")
def ingest(obs: Observation):
    with sqlite3.connect(DB) as db:
        cur = db.execute("INSERT OR IGNORE INTO observations VALUES (?, ?, ?, ?, ?, ?)", (obs.node_id, obs.sequence, obs.observed_at.isoformat(), obs.temperature_c, None if obs.door_open is None else int(obs.door_open), obs.supply_v))
    return {"accepted": cur.rowcount == 1}

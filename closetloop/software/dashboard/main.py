from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3, os

DB = os.getenv("CLOSETLOOP_DB", "closetloop.sqlite3")
app = FastAPI(title="ClosetLoop reference API")
class Event(BaseModel):
    version: int
    node_id: str = Field(min_length=1, max_length=39)
    seq: int = Field(ge=0, le=4294967295)
    event_id: str = Field(min_length=1, max_length=36)
    kind: str
    item_id: str = Field(default="", max_length=39)
    utc_ms: int = Field(ge=0)
    quality: int = Field(ge=0, le=255)

@app.on_event("startup")
def startup():
    with sqlite3.connect(DB) as c:
        c.execute("CREATE TABLE IF NOT EXISTS events (event_id TEXT PRIMARY KEY, node_id TEXT NOT NULL, seq INTEGER NOT NULL, kind TEXT NOT NULL, item_id TEXT NOT NULL, utc_ms INTEGER NOT NULL, quality INTEGER NOT NULL)")

@app.post("/events")
def ingest(e: Event):
    kinds={"worn","needs_repair","washed","drying_complete","mended","retired","door_open","vibration_cycle"}
    if e.version != 1 or e.kind not in kinds or (e.kind not in {"door_open","vibration_cycle"} and not e.item_id):
        raise HTTPException(422, "unsupported event")
    with sqlite3.connect(DB) as c:
        c.execute("INSERT OR IGNORE INTO events VALUES (?,?,?,?,?,?,?)", (e.event_id,e.node_id,e.seq,e.kind,e.item_id,e.utc_ms,e.quality))
    return {"accepted": True, "duplicate_safe": True}

@app.get("/events")
def events(limit: int=100):
    if not 1 <= limit <= 500: raise HTTPException(422, "limit must be 1..500")
    with sqlite3.connect(DB) as c:
        c.row_factory=sqlite3.Row
        return [dict(r) for r in c.execute("SELECT * FROM events ORDER BY utc_ms DESC LIMIT ?",(limit,))]

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3

app = FastAPI(title="Waymark prototype API", version="0.1.0")
DB = "waymark.sqlite3"

class Event(BaseModel):
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=65535)
    kind: str
    value: int = Field(ge=0, le=1)
    unit: str = "bool"

@app.on_event("startup")
def init_db():
    with sqlite3.connect(DB) as db:
        db.execute("CREATE TABLE IF NOT EXISTS events (node TEXT, seq INTEGER, kind TEXT, value INTEGER, unit TEXT, UNIQUE(node,seq))")

@app.get("/health")
def health():
    return {"status": "prototype", "mqtt": "not implemented"}

@app.post("/events")
def ingest(event: Event):
    if event.kind not in {"marker_presence", "inspection_confirmed"}:
        raise HTTPException(422, "unsupported event kind")
    with sqlite3.connect(DB) as db:
        db.execute("INSERT OR IGNORE INTO events VALUES (?,?,?,?,?)", (event.node_id,event.seq,event.kind,event.value,event.unit))
    return {"accepted": True}

@app.get("/events")
def events(limit: int = 100):
    if not 1 <= limit <= 500:
        raise HTTPException(422, "limit must be 1..500")
    with sqlite3.connect(DB) as db:
        rows=db.execute("SELECT node,seq,kind,value,unit FROM events ORDER BY rowid DESC LIMIT ?",(limit,)).fetchall()
    return [{"node_id":r[0],"seq":r[1],"kind":r[2],"value":r[3],"unit":r[4]} for r in rows]

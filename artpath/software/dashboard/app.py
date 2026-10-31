from pathlib import Path
import sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DB = Path(__file__).with_name("artpath.sqlite3")
app = FastAPI(title="ArtPath local prototype", version="0.1.0")

class Readiness(BaseModel):
    v: int = Field(ge=1, le=1)
    type: str = Field(pattern="^ready$")
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=4294967295)
    event_id: str = Field(pattern="^[0-9a-f]{32}$")
    content_rev: str = Field(max_length=64)
    faults: list[str] = Field(default_factory=list, max_length=16)

def connection():
    db = sqlite3.connect(DB)
    db.execute("CREATE TABLE IF NOT EXISTS readiness(node_id TEXT PRIMARY KEY, seq INTEGER, event_id TEXT, content_rev TEXT, faults TEXT, received_at TEXT DEFAULT CURRENT_TIMESTAMP)")
    db.commit()
    return db

@app.get("/health")
def health():
    return {"status": "ok", "mode": "local-prototype"}

@app.get("/content/{content_id}")
def content(content_id: str):
    if not content_id or len(content_id) > 128:
        raise HTTPException(400, "invalid content id")
    return {"content_id": content_id, "available": False, "detail": "Install approved content through the local curator workflow; no sample historical content is asserted."}

@app.post("/readiness")
def readiness(event: Readiness):
    db = connection()
    previous = db.execute("SELECT seq FROM readiness WHERE node_id=?", (event.node_id,)).fetchone()
    if previous and previous[0] == event.seq:
        db.close()
        return {"accepted": True, "duplicate": True}
    db.execute("INSERT INTO readiness(node_id,seq,event_id,content_rev,faults) VALUES(?,?,?,?,?) ON CONFLICT(node_id) DO UPDATE SET seq=excluded.seq,event_id=excluded.event_id,content_rev=excluded.content_rev,faults=excluded.faults,received_at=CURRENT_TIMESTAMP", (event.node_id,event.seq,event.event_id,event.content_rev,",".join(event.faults)))
    db.commit(); db.close()
    return {"accepted": True, "duplicate": False}

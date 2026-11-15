from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import json, sqlite3, os

DB = os.environ.get("BORROWBENCH_DB", "borrowbench.sqlite3")
app = FastAPI(title="BorrowBench local event API", version="0.1.0")
class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0)
    kind: str = Field(pattern="^(presence|weight|inspection)$")
    ts_ms: int = Field(ge=0)
    payload: dict

def connect():
    db = sqlite3.connect(DB)
    db.execute("CREATE TABLE IF NOT EXISTS events (id TEXT, seq INTEGER, kind TEXT, ts_ms INTEGER, payload TEXT, PRIMARY KEY(id,seq))")
    return db

@app.post('/events')
def ingest(event: Event):
    if len(json.dumps(event.payload)) > 512:
        raise HTTPException(413, "payload too large")
    db = connect()
    try:
        db.execute("INSERT INTO events VALUES (?,?,?,?,?)", (event.id,event.seq,event.kind,event.ts_ms,json.dumps(event.payload)))
        db.commit()
    except sqlite3.IntegrityError:
        return {"accepted": True, "duplicate": True}
    finally:
        db.close()
    return {"accepted": True, "duplicate": False}

@app.get('/events')
def recent(limit: int = 100):
    if not 1 <= limit <= 500: raise HTTPException(400, "limit must be 1..500")
    db = connect()
    rows = db.execute("SELECT id,seq,kind,ts_ms,payload FROM events ORDER BY ts_ms DESC LIMIT ?", (limit,)).fetchall()
    db.close()
    return [{"id":r[0],"seq":r[1],"kind":r[2],"ts_ms":r[3],"payload":json.loads(r[4])} for r in rows]

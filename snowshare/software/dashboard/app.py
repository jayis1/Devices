from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3
from pathlib import Path

DB = Path(__file__).with_name("snowshare.sqlite3")
app = FastAPI(title="SnowShare local prototype", version="0.1.0")

class Telemetry(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(pattern=r"^[A-Za-z0-9-]{1,32}$")
    seq: int = Field(ge=0, le=4294967295)
    timestamp: str = Field(min_length=20, max_length=40)
    kind: str = Field(pattern=r"^(ramp|weather)$")
    values: dict[str, float]

def connect():
    db = sqlite3.connect(DB)
    db.execute("CREATE TABLE IF NOT EXISTS telemetry (node TEXT, seq INTEGER, payload TEXT, PRIMARY KEY(node,seq))")
    return db

@app.post("/v1/telemetry")
def ingest(item: Telemetry):
    import json
    with connect() as db:
        latest = db.execute("SELECT MAX(seq) FROM telemetry WHERE node=?", (item.node_id,)).fetchone()[0]
        if latest is not None and item.seq <= latest:
            raise HTTPException(status_code=409, detail="duplicate or stale sequence")
        db.execute("INSERT INTO telemetry VALUES (?,?,?)", (item.node_id, item.seq, item.model_dump_json()))
    return {"accepted": True}

@app.get("/v1/telemetry/{node_id}")
def latest(node_id: str):
    import json
    with connect() as db:
        row = db.execute("SELECT payload FROM telemetry WHERE node=? ORDER BY seq DESC LIMIT 1", (node_id,)).fetchone()
    if row is None:
        raise HTTPException(status_code=404, detail="unknown node")
    return json.loads(row[0])

@app.get("/health")
def health():
    return {"status": "local-prototype"}

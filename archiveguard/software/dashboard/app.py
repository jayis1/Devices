from datetime import datetime
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import sqlite3

app = FastAPI(title="ArchiveGuard API", version="1.0")
DB = "archiveguard.sqlite3"
class Sample(BaseModel):
    schema_version: int = Field(ge=1, le=1)
    site_id: str = Field(min_length=1, max_length=64)
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0)
    captured_at: datetime
    temperature_c: float = Field(ge=-20, le=60)
    relative_humidity_pct: float = Field(ge=0, le=100)

def connect():
    db = sqlite3.connect(DB)
    db.execute("CREATE TABLE IF NOT EXISTS samples (site TEXT, node TEXT, seq INTEGER, payload TEXT, PRIMARY KEY(site,node,seq))")
    return db

@app.post("/v1/telemetry", status_code=202)
def ingest(sample: Sample):
    db = connect()
    try:
        cur = db.execute("INSERT OR IGNORE INTO samples VALUES (?,?,?,?)", (sample.site_id, sample.node_id, sample.seq, sample.model_dump_json()))
        db.commit()
        return {"accepted": cur.rowcount == 1, "duplicate": cur.rowcount == 0}
    finally:
        db.close()

@app.get("/v1/sites/{site_id}/latest")
def latest(site_id: str):
    db = connect()
    try:
        rows = db.execute("SELECT payload FROM samples WHERE site=? ORDER BY seq DESC", (site_id,)).fetchall()
        return [__import__("json").loads(r[0]) for r in rows]
    finally:
        db.close()

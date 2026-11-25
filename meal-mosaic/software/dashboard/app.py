from datetime import datetime, timezone
from pathlib import Path
import sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DB = Path(__file__).with_name("meal_mosaic.sqlite3")
app = FastAPI(title="MealMosaic local prototype", version="0.1.0")
class Observation(BaseModel):
    node_id: str = Field(min_length=1, max_length=40)
    kind: int = Field(ge=1, le=3)
    sequence: int = Field(ge=0)
    value: int = Field(ge=-32768, le=32767)
    status: int = Field(ge=0, le=255)

def connect():
    c=sqlite3.connect(DB); c.execute("CREATE TABLE IF NOT EXISTS observations(node_id TEXT, sequence INTEGER, kind INTEGER, value INTEGER, status INTEGER, received TEXT, PRIMARY KEY(node_id,sequence))"); return c
@app.post("/observations")
def ingest(o: Observation):
    with connect() as db:
        try: db.execute("INSERT INTO observations VALUES(?,?,?,?,?,?)",(o.node_id,o.sequence,o.kind,o.value,o.status,datetime.now(timezone.utc).isoformat()))
        except sqlite3.IntegrityError: raise HTTPException(409,"duplicate node sequence")
    return {"accepted": True}
@app.get("/observations")
def latest():
    with connect() as db:
        rows=db.execute("SELECT node_id,sequence,kind,value,status,received FROM observations ORDER BY received DESC LIMIT 100").fetchall()
    return [{"node_id":r[0],"sequence":r[1],"kind":r[2],"value":r[3],"status":r[4],"received_at":r[5]} for r in rows]

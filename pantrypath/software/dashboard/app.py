from datetime import datetime, timezone
from fastapi import FastAPI
from pydantic import BaseModel, Field
import sqlite3
DB="pantrypath.sqlite3"
app=FastAPI(title="PantryPath prototype")
class Event(BaseModel):
    node_id:int=Field(ge=0,le=255)
    sequence:int=Field(ge=0,le=255)
    observed_at:datetime
    event_type:str=Field(min_length=1,max_length=24)
    value:float|None=None
@app.on_event("startup")
def startup():
    with sqlite3.connect(DB) as db: db.execute("CREATE TABLE IF NOT EXISTS events(node_id INTEGER,sequence INTEGER,observed_at TEXT,event_type TEXT,value REAL,UNIQUE(node_id,sequence))")
@app.get("/health")
def health(): return {"status":"prototype","utc":datetime.now(timezone.utc).isoformat()}
@app.post("/api/v1/events")
def ingest(e:Event):
    with sqlite3.connect(DB) as db:
        cur=db.execute("INSERT OR IGNORE INTO events VALUES(?,?,?,?,?)",(e.node_id,e.sequence,e.observed_at.isoformat(),e.event_type,e.value))
    return {"accepted":cur.rowcount==1}

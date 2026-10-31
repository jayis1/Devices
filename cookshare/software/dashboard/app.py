"""Unauthenticated local prototype; bind to trusted LAN/loopback only."""
from datetime import datetime, timezone
import os
import sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field, field_validator

DB=os.environ.get("COOKSHARE_DB","cookshare.sqlite3")
app=FastAPI(title="CookShare prototype",version="0.1.0")
class Observation(BaseModel):
    node_id:int=Field(gt=0,le=4294967295)
    sequence:int=Field(ge=0,le=65535)
    state:str
    sample_mA:int|None=Field(default=None,ge=0,le=65534)
    observed_at:datetime
    @field_validator("state")
    @classmethod
    def state_ok(cls,v):
        if v not in {"unknown","idle","active"}: raise ValueError("invalid state")
        return v
    @field_validator("observed_at")
    @classmethod
    def tz_required(cls,v):
        if v.tzinfo is None: raise ValueError("timezone required")
        return v

def db():
    c=sqlite3.connect(DB)
    c.execute("CREATE TABLE IF NOT EXISTS obs(node_id INTEGER,sequence INTEGER,state TEXT,sample_mA INTEGER,observed_at TEXT,PRIMARY KEY(node_id,sequence))")
    return c
@app.get("/health")
def health(): return {"status":"ok"}
@app.post("/v1/observations",status_code=202)
def ingest(o:Observation):
    with db() as c:
        cur=c.execute("INSERT OR IGNORE INTO obs VALUES(?,?,?,?,?)",(o.node_id,o.sequence,o.state,o.sample_mA,o.observed_at.astimezone(timezone.utc).isoformat()))
        if cur.rowcount==0:return {"status":"duplicate"}
    return {"status":"accepted"}
def latest(node=None):
    with db() as c: rows=c.execute("SELECT node_id,sequence,state,sample_mA,observed_at FROM obs"+(" WHERE node_id=?" if node else "")+" ORDER BY observed_at DESC",(node,) if node else ()).fetchall()
    seen=set();out=[];now=datetime.now(timezone.utc)
    for r in rows:
        if r[0] in seen:continue
        seen.add(r[0]); age=(now-datetime.fromisoformat(r[4])).total_seconds()
        out.append({"node_id":r[0],"sequence":r[1],"state":r[2] if 0<=age<=90 else "unknown","sample_mA":r[3],"observed_at":r[4]})
    return out
@app.get("/v1/stations")
def stations():return latest()
@app.get("/v1/stations/{node_id}")
def station(node_id:int):
    r=latest(node_id)
    if not r:raise HTTPException(404,"station not found")
    return r[0]

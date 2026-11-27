from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Literal
from datetime import datetime, timezone
app=FastAPI(title="DeskReady local API", version="1.0.0")
state={}
class Event(BaseModel):
    v: Literal[1]
    node: str=Field(min_length=1,max_length=24,pattern=r"^[A-Za-z0-9_-]+$")
    seq: int=Field(ge=0,le=4294967295)
    kind: str=Field(min_length=1,max_length=16)
    ready: bool
    uptime_s: int=Field(ge=0)
@app.get('/health')
def health(): return {'ok':True}
@app.post('/events')
def ingest(e:Event):
    prev=state.get(e.node)
    if prev and e.seq<=prev['seq']: raise HTTPException(409,'duplicate or stale sequence')
    state[e.node]={**e.model_dump(),'received_at':datetime.now(timezone.utc).isoformat()}
    return {'accepted':True}
@app.get('/stations')
def stations(): return state

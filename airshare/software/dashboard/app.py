from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime, timezone
from typing import Optional
app = FastAPI(title="AirShare prototype API", version="0.1")
class Observation(BaseModel):
    schema_version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=64)
    sequence: int = Field(ge=0, le=4294967295)
    captured_at: datetime
    co2_ppm: Optional[int] = Field(default=None, ge=0, le=10000)
    temperature_c: Optional[float] = Field(default=None, ge=-40, le=85)
    relative_humidity_pct: Optional[float] = Field(default=None, ge=0, le=100)
    door_open: Optional[bool] = None
store = {}
@app.get('/health')
def health(): return {'status':'ok'}
@app.post('/v1/observations')
def ingest(o: Observation):
    if all(getattr(o,k) is None for k in ('co2_ppm','temperature_c','relative_humidity_pct','door_open')): raise HTTPException(422,'observation has no readings')
    key=o.node_id
    old=store.get(key)
    if old and o.sequence <= old['sequence']: raise HTTPException(409,'duplicate or out-of-order sequence')
    store[key]=o.model_dump(mode='json')
    return {'accepted':True,'node_id':key}
@app.get('/v1/sites/{site_id}/latest')
def latest(site_id: str): return {'site_id':site_id,'observations':list(store.values()),'as_of':datetime.now(timezone.utc).isoformat()}

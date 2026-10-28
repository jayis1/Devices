from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="ParcelPause prototype")
latest = {}
class Reading(BaseModel):
    node_id: int = Field(ge=1, le=4294967295)
    node_type: int = Field(ge=1, le=2)
    sequence: int = Field(ge=0, le=4294967295)
    age_ms: int = Field(ge=0, le=86400000)
    value: int = Field(ge=0, le=65535)
    status: int = Field(ge=0, le=255)
@app.get('/health')
def health(): return {"status":"ok"}
@app.post('/readings', status_code=202)
def ingest(r: Reading):
    key=(r.node_id,r.node_type)
    old=latest.get(key)
    if old and r.sequence == old.sequence: raise HTTPException(409,"duplicate")
    latest[key]=r
    return {"accepted":True}
@app.get('/readings')
def readings(): return [x.model_dump() for x in latest.values()]

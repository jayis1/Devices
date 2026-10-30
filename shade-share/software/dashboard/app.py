from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Dict
import os

app = FastAPI(title="ShadeShare local status API", version="0.1.0")
records: Dict[int, dict] = {}
class Reading(BaseModel):
    node_id: int = Field(ge=0, le=4294967295)
    seq: int = Field(ge=0, le=65535)
    kind: int = Field(ge=1, le=4)
    value: int = Field(ge=-32768, le=32767)
    battery_mv: int = Field(ge=0, le=65535)

@app.get('/health')
def health(): return {'ok': True, 'mode': 'local-reference'}

@app.post('/readings')
def ingest(r: Reading):
    old = records.get(r.node_id)
    if old and old['seq'] == r.seq: raise HTTPException(409, 'duplicate sequence')
    records[r.node_id] = r.model_dump()
    return {'accepted': True}

@app.get('/readings')
def readings(): return list(records.values())

# MQTT TLS/auth wiring and persistent storage intentionally remain deployment work.
BROKER_URL = os.getenv('MQTT_URL', '')

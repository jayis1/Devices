from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Dict
app = FastAPI(title="SpokeSense local telemetry")
class Reading(BaseModel):
    node_id: str = Field(min_length=1, max_length=15)
    sequence: int = Field(ge=0, le=4294967295)
    kind: str
    value: float
    unit: str
latest: Dict[str, Reading] = {}
@app.post('/v1/telemetry')
def ingest(r: Reading):
    if r.kind not in {'wheel_deviation','lever_force','pressure'}:
        raise HTTPException(422, 'unsupported measurement kind')
    old=latest.get(r.node_id)
    if old and r.sequence <= old.sequence:
        raise HTTPException(409, 'duplicate or stale sequence')
    latest[r.node_id]=r
    return {'accepted': True, 'node_id': r.node_id, 'sequence': r.sequence}
@app.get('/v1/status')
def status():
    return {'readings': [r.model_dump() for r in latest.values()], 'safety_assessment': None}

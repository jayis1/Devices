from fastapi import FastAPI
from pydantic import BaseModel, Field
from time import monotonic
app = FastAPI(title="SpinShare local status API")
class Report(BaseModel):
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    vibration_rms_mg: int = Field(ge=0, le=65535)
    door_open: bool
    received_at: float
cache = {}
@app.get('/health')
def health(): return {'ok': True}
@app.post('/v1/report')
def report(r: Report):
    cache[r.node_id] = (r, monotonic())
    return {'accepted': True}
@app.get('/v1/machines')
def machines():
    out=[]
    for node,(r,seen) in cache.items():
        age=monotonic()-seen
        state='offline' if age>90 else ('running' if r.vibration_rms_mg>120 else ('available' if r.door_open else 'idle_unknown'))
        out.append({'node_id':node,'state':state,'updated_age_s':round(age,1)})
    return out

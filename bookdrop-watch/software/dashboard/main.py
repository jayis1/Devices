from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from collections import deque

app = FastAPI(title="BookDrop Watch prototype")
events = deque(maxlen=5000)

class Telemetry(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=4294967295)
    kind: str = Field(min_length=1, max_length=32)
    value: int | float | bool

@app.get('/health')
def health():
    return {'status': 'ok', 'stored': len(events)}

@app.post('/telemetry')
def telemetry(message: Telemetry):
    if message.kind not in {'passage', 'flap', 'bin_mass_g', 'heartbeat'}:
        raise HTTPException(400, 'unsupported kind')
    events.append(message.model_dump())
    return {'accepted': True}

@app.get('/nodes')
def nodes():
    return sorted({item['node'] for item in events})

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from time import time
from collections import deque

app = FastAPI(title="QuietHours local API", version="0.1.0")
events = deque(maxlen=10000)
class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    id: str = Field(min_length=1, max_length=40, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=2**32-1)
    t: int = Field(ge=0)
    kind: str = Field(pattern=r"^(level|mode|ack)$")
    db10: int | None = Field(default=None, ge=0, le=1200)

@app.post('/v1/events')
def ingest(event: Event):
    if event.kind == 'level' and event.db10 is None:
        raise HTTPException(422, 'level requires db10')
    events.append({**event.model_dump(), 'received': int(time())})
    return {'accepted': True}

@app.get('/health')
def health():
    return {'status': 'ok', 'buffered': len(events), 'persistent': False}

@app.get('/v1/events')
def list_events():
    return list(events)

@app.delete('/v1/events')
def delete_events():
    events.clear()
    return {'deleted': True}

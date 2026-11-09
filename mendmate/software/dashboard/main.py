from datetime import datetime, timezone
from typing import Any
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="MendMate local prototype", docs_url=None, redoc_url=None)
latest: dict[str, dict[str, Any]] = {}
repairs: dict[str, dict[str, str]] = {}

class Event(BaseModel):
    schema_: str = Field(alias="schema", pattern=r"^mendmate\.telemetry\.v1$")
    device_id: str = Field(min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")
    boot_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=2**32-1)
    observed_at: datetime
    kind: str
    payload: dict[str, Any]

class Repair(BaseModel):
    garment: str = Field(min_length=1, max_length=100)
    damage: str = Field(min_length=1, max_length=200)
    fabric_label: str | None = Field(default=None, max_length=80)

@app.get('/health')
def health(): return {'status':'ok'}

@app.post('/v1/events')
def ingest(event: Event):
    if event.kind == 'swatch':
        channels = event.payload.get('channels')
        if not isinstance(channels, list) or len(channels) != 11 or any(type(v) is not int or not 0 <= v <= 65535 for v in channels):
            raise HTTPException(422, 'invalid swatch channels')
    elif event.kind == 'station':
        if type(event.payload.get('thread_cg')) is not int or not 0 <= event.payload['thread_cg'] <= 500000 or type(event.payload.get('drawer_open')) is not bool:
            raise HTTPException(422, 'invalid station payload')
    else: raise HTTPException(422, 'unknown event kind')
    previous = latest.get(event.device_id)
    if previous and previous['boot_id'] == event.boot_id and event.seq <= previous['seq']:
        raise HTTPException(409, 'duplicate or stale sequence')
    latest[event.device_id] = {'device_id':event.device_id,'boot_id':event.boot_id,'seq':event.seq,
        'kind':event.kind,'payload':event.payload,'observed_at':event.observed_at.isoformat()}
    return {'accepted': True}

@app.get('/v1/status')
def status():
    now=datetime.now(timezone.utc)
    return {'nodes':[dict(v, age_seconds=max(0,(now-datetime.fromisoformat(v['observed_at']).astimezone(timezone.utc)).total_seconds())) for v in latest.values()]}

@app.post('/v1/repairs')
def create_repair(repair: Repair):
    key=f'repair-{len(repairs)+1}'
    repairs[key]=repair.model_dump()
    return {'id':key,'repair':repairs[key]}

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime, timezone

app = FastAPI(title="ListenLink local readiness API", version="0.1.0")
state = {}
class Reading(BaseModel):
    site: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9_-]+$")
    room: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9_-]+$")
    sequence: int = Field(ge=0)
    transmitter_on: bool
    receiver_count: int = Field(ge=0, le=8)
    observed_at: datetime

@app.get('/health')
def health(): return {'status':'ok'}
@app.post('/v1/readings')
def ingest(r: Reading):
    key=(r.site,r.room); old=state.get(key)
    if old and r.sequence <= old['sequence']: raise HTTPException(409,'duplicate or stale sequence')
    state[key]=r.model_dump(mode='json'); return {'accepted':True}
@app.get('/v1/readiness')
def readiness():
    now=datetime.now(timezone.utc); out=[]
    for r in state.values():
        observed=datetime.fromisoformat(r['observed_at'].replace('Z','+00:00'))
        fresh=(now-observed).total_seconds() <= 120
        out.append({'site':r['site'],'room':r['room'],'status':'ready' if fresh and r['transmitter_on'] and r['receiver_count'] else 'not_ready' if fresh else 'unknown','receiver_count':r['receiver_count'] if fresh else None})
    return out

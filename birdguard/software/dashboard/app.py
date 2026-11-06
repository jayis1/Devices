from datetime import datetime, timezone
from math import isfinite
from pathlib import Path
import json, sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field, ConfigDict

DB = Path(__file__).with_name('observations.sqlite3')
app = FastAPI(title='BirdGuard local prototype', version='0.1.0')
class Observation(BaseModel):
    model_config = ConfigDict(extra='ignore')
    version: int = Field(ge=1, le=1)
    device_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0)
    ts: datetime
    kind: str
    values: dict

def db():
    c = sqlite3.connect(DB)
    c.execute('CREATE TABLE IF NOT EXISTS observations (device_id TEXT, seq INTEGER, ts TEXT, kind TEXT, values_json TEXT, PRIMARY KEY(device_id,seq))')
    return c
@app.get('/health')
def health(): return {'status':'ok'}
@app.post('/v1/observations')
def ingest(o: Observation):
    if o.ts.tzinfo is None or o.ts > datetime.now(timezone.utc): raise HTTPException(422, 'timestamp must be timezone-aware and not future')
    v=o.values
    limits={'lux':(0,200000)}
    if o.kind=='window':
        if not isinstance(v.get('panel_present'), bool): raise HTTPException(422,'panel_present must be boolean')
    elif o.kind=='weather': limits.update({'temperature_c':(-40,85),'humidity_pct':(0,100),'pressure_hpa':(300,1200)})
    else: raise HTTPException(422,'unknown kind')
    for k,(lo,hi) in limits.items():
        x=v.get(k)
        if isinstance(x,bool) or not isinstance(x,(int,float)) or not isfinite(x) or not lo<=x<=hi: raise HTTPException(422,f'invalid {k}')
    try:
        with db() as c: c.execute('INSERT INTO observations VALUES (?,?,?,?,?)',(o.device_id,o.seq,o.ts.isoformat(),o.kind,json.dumps(v)))
    except sqlite3.IntegrityError: raise HTTPException(409,'duplicate device sequence')
    return {'accepted':True}
@app.get('/v1/status')
def status():
    with db() as c: rows=c.execute('SELECT device_id,seq,ts,kind,values_json FROM observations ORDER BY ts DESC LIMIT 100').fetchall()
    return {'observations':[{'device_id':d,'seq':s,'ts':t,'kind':k,'values':json.loads(v)} for d,s,t,k,v in rows]}

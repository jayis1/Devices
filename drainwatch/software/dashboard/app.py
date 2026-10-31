from datetime import datetime, timezone
from pathlib import Path
import os, sqlite3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DB = Path(os.getenv('DRAINWATCH_DB', '/tmp/drainwatch.sqlite3'))
app = FastAPI(title='DrainWatch prototype', version='0.1.0')

def connect():
    DB.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(DB)
    con.execute('CREATE TABLE IF NOT EXISTS events (node_id INTEGER NOT NULL, sequence INTEGER NOT NULL, kind TEXT NOT NULL, value INTEGER NOT NULL, received_at TEXT NOT NULL, valid INTEGER NOT NULL, PRIMARY KEY(node_id, sequence))')
    return con

class Event(BaseModel):
    node_id: int = Field(ge=0, le=4294967295)
    sequence: int = Field(ge=0, le=4294967295)
    kind: str
    value: int = Field(ge=-1000000, le=1000000)
    received_at: datetime
    valid: bool = True

@app.get('/health')
def health(): return {'status':'ok'}

@app.post('/events')
def ingest(e: Event):
    if e.kind not in ('inlet','rain') or (e.kind == 'inlet' and not 0 <= e.value <= 4000) or (e.kind == 'rain' and not 0 <= e.value <= 10000):
        raise HTTPException(422, 'invalid kind/value')
    stamp = e.received_at.astimezone(timezone.utc).isoformat()
    try:
        with connect() as c:
            c.execute('INSERT INTO events VALUES (?,?,?,?,?,?)',(e.node_id,e.sequence,e.kind,e.value,stamp,int(e.valid)))
    except sqlite3.IntegrityError:
        return {'accepted':False,'duplicate':True}
    return {'accepted':True,'duplicate':False}

@app.get('/nodes')
def nodes():
    with connect() as c:
        rows=c.execute('SELECT e.node_id,e.sequence,e.kind,e.value,e.received_at,e.valid FROM events e JOIN (SELECT node_id,MAX(rowid) r FROM events GROUP BY node_id) x ON e.rowid=x.r').fetchall()
    return [{'node_id':r[0],'sequence':r[1],'kind':r[2],'value':r[3],'received_at':r[4],'valid':bool(r[5])} for r in rows]

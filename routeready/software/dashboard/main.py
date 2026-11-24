from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import os, sqlite3

DB = os.environ.get("ROUTEREADY_DB", "routeready.sqlite3")
app = FastAPI(title="RouteReady local status API")
class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    kind: str = Field(pattern=r"^(section_state|route_checkin|battery_mv)$")
    value: str | int = Field()
    utc_ms: int = Field(ge=0)

def db():
    c=sqlite3.connect(DB)
    c.execute("CREATE TABLE IF NOT EXISTS events(node TEXT,seq INTEGER,kind TEXT,value TEXT,received INTEGER DEFAULT (unixepoch()),PRIMARY KEY(node,seq))")
    return c
@app.post('/events')
def ingest(e: Event):
    if e.kind == 'section_state' and e.value not in ('open','setting','closed'): raise HTTPException(422,'invalid section state')
    if e.kind == 'battery_mv' and (not isinstance(e.value,int) or isinstance(e.value,bool) or not 0 <= e.value <= 6000): raise HTTPException(422,'invalid battery voltage')
    if e.kind == 'route_checkin' and (not isinstance(e.value,str) or len(e.value)>32 or not e.value.isalnum()): raise HTTPException(422,'invalid route identifier')
    try:
        with db() as c: c.execute('INSERT INTO events(node,seq,kind,value) VALUES(?,?,?,?)',(e.node,e.seq,e.kind,str(e.value)))
    except sqlite3.IntegrityError: return {'accepted':False,'duplicate':True}
    return {'accepted':True,'duplicate':False}
@app.get('/events')
def events():
    with db() as c: rows=c.execute('SELECT node,seq,kind,value,received FROM events ORDER BY received DESC LIMIT 200').fetchall()
    return [{'node':r[0],'seq':r[1],'kind':r[2],'value':r[3],'received':r[4]} for r in rows]

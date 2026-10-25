import json, sqlite3
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app=FastAPI(title='MealBridge prototype', version='0.1.0')
db=sqlite3.connect('mealbridge.sqlite3', check_same_thread=False)
db.execute('CREATE TABLE IF NOT EXISTS telemetry(node TEXT, seq INTEGER, kind TEXT, value REAL, unit TEXT, ts TEXT, PRIMARY KEY(node,seq))')
class Event(BaseModel):
    v:int=Field(ge=1,le=1)
    node_id:str=Field(min_length=1,max_length=48)
    seq:int=Field(ge=0)
    ts_utc:datetime
    kind:str=Field(min_length=1,max_length=32)
    value:float
    unit:str=Field(min_length=1,max_length=16)
@app.get('/health')
def health(): return {'status':'ok','storage':'sqlite'}
@app.post('/telemetry')
def telemetry(event:Event):
    if event.ts_utc.tzinfo is None: raise HTTPException(400,'timestamp must include timezone')
    try:
        db.execute('INSERT INTO telemetry VALUES (?,?,?,?,?,?)',(event.node_id,event.seq,event.kind,event.value,event.unit,event.ts_utc.astimezone(timezone.utc).isoformat()))
        db.commit()
    except sqlite3.IntegrityError: return {'accepted':False,'duplicate':True}
    return {'accepted':True,'duplicate':False}
@app.get('/telemetry')
def recent(limit:int=100):
    if not 1<=limit<=500: raise HTTPException(400,'limit out of range')
    rows=db.execute('SELECT node,seq,kind,value,unit,ts FROM telemetry ORDER BY rowid DESC LIMIT ?', (limit,)).fetchall()
    return [dict(zip(('node_id','seq','kind','value','unit','ts_utc'),r)) for r in rows]

from fastapi import FastAPI
from pydantic import BaseModel, Field
app=FastAPI(title='BusPulse local prototype')
latest={}
class Event(BaseModel):
    node_id:int=Field(ge=1,le=255)
    sequence:int=Field(ge=0,le=65535)
    timestamp:int=Field(ge=0)
    value_mv:int=Field(ge=0,le=65535)
    temperature_centi_c:int=Field(ge=-32768,le=32767)
    valid:bool
@app.get('/health')
def health(): return {'status':'ok'}
@app.post('/telemetry')
def telemetry(e:Event):
    latest[e.node_id]=e.model_dump()
    return {'accepted':True}
@app.get('/nodes')
def nodes(): return {'nodes':list(latest.values())}

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app=FastAPI(title='LineWise API',version='1.0')
class Reading(BaseModel):
 v:int=Field(ge=1,le=1); node_id:str=Field(min_length=1,max_length=32); seq:int=Field(ge=0,le=4294967295); kind:str=Field(pattern='^(line_load_g|rain_tips|wind_mm_s)$'); value:int; unit:str=Field(max_length=12)
_latest={}
@app.post('/v1/telemetry')
def ingest(r:Reading):
 old=_latest.get(r.node_id)
 if old and r.seq<=old['seq']: raise HTTPException(409,'sequence must increase')
 _latest[r.node_id]=r.model_dump(); return {'accepted':True}
@app.get('/v1/status')
def status(): return {'nodes':list(_latest.values())}
@app.get('/health')
def health(): return {'ok':True}

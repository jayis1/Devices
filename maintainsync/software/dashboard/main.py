import os
from datetime import datetime, timezone
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field
app=FastAPI(title="MaintainSync API")
TOKEN=os.getenv("MAINTAINSYNC_API_TOKEN","change-me")
assets={}; cards=[]
class Event(BaseModel): node_id:int; asset_id:str; kind:str; data:dict={}
class Interlock(BaseModel): node_id:int; command:str; ttl_s:int=Field(gt=0,le=30); local_key_confirmed:bool
def auth(v:str|None):
 if v!=f"Bearer {TOKEN}": raise HTTPException(401,"unauthorized")
@app.get('/health')
def health(): return {'status':'ok','time':datetime.now(timezone.utc).isoformat()}
@app.get('/v1/assets')
def get_assets(authorization:str|None=None): auth(authorization);return assets
@app.get('/v1/cards')
def get_cards(authorization:str|None=None): auth(authorization);return cards
@app.post('/v1/events',status_code=202)
def event(e:Event,authorization:str|None=None):
 auth(authorization); at=datetime.now(timezone.utc).isoformat();assets[e.asset_id]={'node_id':e.node_id,'kind':e.kind,'data':e.data,'at':at}
 if e.kind in {'anomaly','leak_candidate','filter_restricted'}: cards.append({'asset_id':e.asset_id,'evidence':e.data,'created_at':at,'status':'review'})
 return {'accepted':True}
@app.post('/v1/interlocks/request',status_code=202)
def interlock(r:Interlock,authorization:str|None=None):
 auth(authorization)
 if not r.local_key_confirmed: raise HTTPException(409,'local hardware key confirmation required')
 return {'accepted':True,'delivery':'policy-pending','ttl_s':r.ttl_s}

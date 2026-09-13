import os
from datetime import datetime, timezone
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field
app=FastAPI(title="HandiSync API")
TOKEN=os.getenv("HANDISYNC_API_TOKEN", "change-me")
nodes={}
class Command(BaseModel): node_id:int; action:str; duration_s:int=Field(gt=0,le=900); confirmation:bool
class Event(BaseModel): node_id:int; kind:str; data:dict={}
def auth(v:str|None):
 if v != f"Bearer {TOKEN}": raise HTTPException(401,"unauthorized")
@app.get('/health')
def health(): return {'status':'ok','time':datetime.now(timezone.utc).isoformat()}
@app.get('/v1/nodes')
def get_nodes(authorization:str|None=Header(None)): auth(authorization); return nodes
@app.post('/v1/events',status_code=202)
def event(e:Event,authorization:str|None=Header(None)):
 auth(authorization); nodes[e.node_id]={'kind':e.kind,'data':e.data,'at':datetime.now(timezone.utc).isoformat()}; return {'accepted':True}
@app.post('/v1/commands',status_code=202)
def command(c:Command,authorization:str|None=Header(None)):
 auth(authorization)
 if not c.confirmation: raise HTTPException(409,'local confirmation required')
 return {'accepted':True,'delivery':'hub-policy-pending'}

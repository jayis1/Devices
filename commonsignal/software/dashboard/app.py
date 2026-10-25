from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title='CommonSignal prototype')
class Observation(BaseModel):
    site_id: str = Field(min_length=1, max_length=48)
    node_type: str = Field(min_length=1, max_length=32)
    node_id: str = Field(min_length=1, max_length=32)
    kind: str = Field(min_length=1, max_length=24)
    value: dict
@app.get('/health')
def health(): return {'status':'ok'}
@app.post('/telemetry')
def telemetry(item: Observation):
    if item.kind not in {'route','equipment','climate','fault','heartbeat'}: raise HTTPException(422,'unsupported kind')
    return {'accepted':True,'site_id':item.site_id,'kind':item.kind}

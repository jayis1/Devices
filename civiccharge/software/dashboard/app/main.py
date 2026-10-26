from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Dict

app = FastAPI(title="CivicCharge local prototype", version="0.1.0")

class Outlet(BaseModel):
    node_id: str
    voltage_v_rms: float = Field(ge=0, le=300)
    current_ma: int = Field(ge=0, le=30000)
    temp_c: float = Field(ge=-40, le=125)
    flags: int = Field(ge=0, le=65535)
    online: bool = True

states: Dict[str, Outlet] = {}

@app.get("/health")
def health():
    return {"ok": True}

@app.get("/v1/outlets")
def outlets():
    return list(states.values())

@app.put("/v1/outlets/{node_id}")
def update(node_id: str, outlet: Outlet):
    if node_id != outlet.node_id:
        raise HTTPException(status_code=400, detail="node_id mismatch")
    states[node_id] = outlet
    return outlet

@app.post("/v1/outlets/{node_id}/inspection")
def inspection(node_id: str):
    if node_id not in states:
        raise HTTPException(status_code=404, detail="unknown outlet")
    return {"node_id": node_id, "inspection_recorded": True}

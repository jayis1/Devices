from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, ConfigDict, Field
from typing import Literal

app = FastAPI(title="UmbrellaLink prototype", version="0.1.0")

class Reading(BaseModel):
    model_config = ConfigDict(extra="forbid")
    v: Literal[1]
    node: str = Field(pattern=r"^(lend|return)-[0-9]{2}$")
    seq: int = Field(ge=0, le=4294967295)
    kind: Literal["slots"]
    free: int = Field(ge=0, le=4)
    total: int = Field(ge=0, le=4)
    uptime_s: int = Field(ge=0, le=4294967295)

state: dict[str, dict] = {}

@app.post("/v1/readings")
def ingest(r: Reading):
    if r.free > r.total:
        raise HTTPException(422, "free cannot exceed total")
    old = state.get(r.node)
    if old is not None and r.seq <= old["seq"]:
        return {"accepted": False, "reason": "duplicate_or_out_of_order"}
    state[r.node] = {**r.model_dump(), "received_at": "gateway-time-not-configured"}
    return {"accepted": True}

@app.get("/v1/sites/demo")
def site():
    return {"site": "demo", "docks": list(state.values()), "prototype": True}

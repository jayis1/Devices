from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Literal

app = FastAPI(title="PlayShare local readiness prototype")
events = {}

class Event(BaseModel):
    v: Literal[1]
    id: str = Field(min_length=1, max_length=32, pattern=r"^[A-Za-z0-9-]+$")
    node: Literal["rack", "dock", "inspection"]
    seq: int = Field(ge=0, le=4294967295)
    ts_ms: int = Field(ge=0)
    kind: Literal["stock", "return_fill", "inspection"]
    value: float = Field(ge=0, le=1000000)
    unit: Literal["items", "percent", "event"]

@app.post("/events")
def ingest(event: Event):
    if (event.node, event.kind) not in [("rack", "stock"), ("dock", "return_fill"), ("inspection", "inspection")]:
        raise HTTPException(422, "kind does not match node")
    key = event.id
    previous = events.get(key)
    if previous and event.seq <= previous["seq"]:
        return {"accepted": False, "reason": "duplicate_or_out_of_order"}
    events[key] = {**event.model_dump(), "received": True}
    return {"accepted": True}

@app.get("/readiness")
def readiness():
    return {"nodes": list(events.values()), "count": len(events), "mode": "prototype-local-memory"}

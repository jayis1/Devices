from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from collections import deque

app = FastAPI(title="NeighborNest local API", version="0.1.0")
events = deque(maxlen=2048)

class Observation(BaseModel):
    node_id: str = Field(min_length=1, max_length=32)
    kind: str = Field(pattern="^(soil|visit|health)$")
    value: float | None = None
    unit: str | None = Field(default=None, max_length=16)
    observed_at: str = Field(min_length=1, max_length=40)

@app.get("/health")
def health():
    return {"status": "ok", "stored": len(events), "storage": "in-memory demo; replace with SQLite"}

@app.post("/observations", status_code=202)
def ingest(item: Observation):
    events.append(item.model_dump())
    return {"accepted": True, "count": len(events)}

@app.get("/observations")
def read_observations():
    return list(events)

@app.delete("/observations")
def clear():
    events.clear()
    return {"deleted": True}

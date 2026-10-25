"""Demo-only ShelterSignal API; in-memory and unauthenticated, loopback use only."""
from collections import deque
from typing import Literal
from fastapi import FastAPI
from pydantic import BaseModel, Field
app = FastAPI(title="ShelterSignal demo", version="0.1.0")
_events = deque(maxlen=500)
class Event(BaseModel):
    node_id: int = Field(ge=1, le=65535)
    kind: Literal["light", "contact", "approach", "heartbeat"]
    sequence: int = Field(ge=0, le=4294967295)
    age_s: int = Field(ge=0, le=4294967295)
    value: int = Field(ge=-2147483648, le=2147483647)
@app.get("/health")
def health(): return {"status": "demo", "events": len(_events)}
@app.post("/events")
def ingest(event: Event):
    _events.append(event.model_dump())
    return {"accepted": True}
@app.get("/events")
def events(): return list(_events)

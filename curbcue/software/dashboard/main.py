from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Literal

app = FastAPI(title="CurbCue prototype", version="0.1.0")
seen: set[tuple[str, int]] = set()
events: list[dict] = []

class Event(BaseModel):
    v: Literal[1]
    node_id: str = Field(min_length=1, max_length=32, pattern=r"^[A-Za-z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    kind: Literal["obstruction", "wet", "tilt", "manual", "ack"]
    sample_ms: int = Field(ge=0, le=4294967295)
    severity: int = Field(ge=0, le=3)

@app.post("/events", status_code=202)
def ingest(event: Event):
    key = (event.node_id, event.seq)
    if key in seen:
        return {"accepted": False, "duplicate": True}
    seen.add(key)
    events.append(event.model_dump())
    return {"accepted": True, "verified": False}

@app.get("/events")
def list_events():
    return {"events": events[-500:], "warning": "unverified advisory data; not a route safety service"}

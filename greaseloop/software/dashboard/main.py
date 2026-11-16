from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, ConfigDict, Field
from typing import Literal

app = FastAPI(title="GreaseLoop local API", version="0.1.0")
_events: list[dict] = []
_seen: set[tuple[str, int]] = set()

class Event(BaseModel):
    model_config = ConfigDict(extra="forbid")
    v: Literal[1]
    node: str = Field(min_length=1, max_length=48, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0)
    ts: int = Field(ge=0)
    kind: Literal["deposit", "collection", "container_status", "environment"]
    mass_g: int = Field(ge=-500, le=20000)

@app.get("/health")
def health():
    return {"status": "ok", "mode": "in-memory-prototype"}

@app.post("/events")
def ingest(event: Event):
    key = (event.node, event.seq)
    if key in _seen:
        return {"accepted": False, "duplicate": True}
    _seen.add(key)
    _events.append(event.model_dump())
    return {"accepted": True, "duplicate": False}

@app.get("/events")
def events(limit: int = 100):
    if not 1 <= limit <= 500:
        raise HTTPException(400, "limit must be 1..500")
    return {"events": _events[-limit:]}

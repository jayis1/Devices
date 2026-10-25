from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="LiftLink prototype", docs_url=None, redoc_url=None)

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=4294967295)
    type: int = Field(ge=1, le=4)
    uptime_ms: int = Field(ge=0, le=4294967295)
    status: int = Field(ge=0, le=4)
    confidence: int = Field(ge=0, le=100)

_events: dict[tuple[int, int], dict] = {}

@app.get("/health")
def health():
    return {"ok": True, "mode": "prototype-local"}

@app.post("/events")
def ingest(event: Event):
    key = (event.node_id, event.sequence)
    duplicate = key in _events
    _events.setdefault(key, event.model_dump())
    return {"accepted": True, "duplicate": duplicate}

@app.get("/sites/{site_id}/status")
def status(site_id: str):
    if not site_id or len(site_id) > 64:
        raise HTTPException(status_code=400, detail="invalid site")
    return {"site_id": site_id, "status": "unknown", "freshness": "unknown", "provenance": "prototype"}

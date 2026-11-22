from fastapi import FastAPI, Query
from pydantic import BaseModel, Field

app = FastAPI(title="ChargePlan local reference API", version="0.1.0")
_events: dict[tuple[str, int], dict] = {}

class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=32)
    seq: int = Field(ge=0)
    ts: int = Field(ge=0)
    kind: str
    state: str | None = None
    energy_wh: int | None = Field(default=None, ge=0)
    watts: int | None = Field(default=None, ge=0)

@app.get("/health")
def health():
    return {"status": "ok", "stored_events": len(_events)}

@app.post("/events", status_code=202)
def ingest(event: Event):
    if event.kind not in {"session", "power"}:
        return {"accepted": False, "reason": "unsupported kind"}
    key = (event.node, event.seq)
    if key in _events:
        return {"accepted": True, "duplicate": True}
    _events[key] = event.model_dump()
    return {"accepted": True, "duplicate": False}

@app.get("/events")
def events(limit: int = Query(default=100, ge=1, le=500)):
    return list(_events.values())[-limit:]

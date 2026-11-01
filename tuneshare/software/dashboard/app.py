from fastapi import FastAPI
from pydantic import BaseModel, Field

app = FastAPI(title="TuneShare local reference API")

class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=4294967295)
    ts: int | None = None
    kind: str = Field(pattern="^(climate|rack|checkin)$")
    data: dict

_events: dict[tuple[str, int], Event] = {}

@app.post("/v1/events", status_code=202)
def ingest(event: Event):
    key = (event.node_id, event.seq)
    duplicate = key in _events
    if not duplicate:
        _events[key] = event
    return {"accepted": True, "duplicate": duplicate}

@app.get("/v1/events")
def list_events():
    return list(_events.values())

@app.get("/health")
def health():
    return {"status": "ok", "storage": "process-memory demo"}

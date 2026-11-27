# FastAPI local event reference: storage is in-memory and not production-ready.
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="ParkKeys local reference")
events: dict[str, dict] = {}

class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    event_id: str
    node_id: str
    seq: int = Field(ge=0)
    observed_at: str
    kind: str
    value: dict

@app.post("/events")
def post_event(event: Event):
    if event.kind not in {"piano_report", "cover_state", "weather"}:
        raise HTTPException(422, "unsupported event kind")
    duplicate = event.event_id in events
    events.setdefault(event.event_id, event.model_dump())
    return {"accepted": True, "duplicate": duplicate}

@app.get("/reports")
def reports():
    return list(events.values())

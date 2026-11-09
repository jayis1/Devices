from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="MealMate prototype")
_events: list[dict] = []

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    device_id: int = Field(gt=0)
    sequence: int = Field(ge=0)
    kind: str
    value_a: int
    value_b: int = 0

@app.post("/v1/events", status_code=202)
def ingest(event: Event):
    if event.kind not in {"weight", "climate", "door"}:
        raise HTTPException(400, "unsupported kind")
    if len(_events) >= 10000:
        raise HTTPException(503, "prototype buffer full")
    if any(x["device_id"] == event.device_id and x["sequence"] == event.sequence for x in _events):
        return {"accepted": True, "duplicate": True}
    _events.append(event.model_dump())
    return {"accepted": True, "duplicate": False}

@app.get("/v1/events")
def events():
    return _events

"""Prototype API; in-memory only, not exposed securely for production."""
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="LabSignal prototype")
events = []
class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    kind: int = Field(ge=1, le=2)
    seq: int = Field(ge=0, le=4294967295)
    value_milli: int = Field(ge=-2147483648, le=2147483647)
@app.get("/health")
def health():
    return {"status": "prototype", "stored": len(events)}
@app.post("/events", status_code=202)
def post_event(event: Event):
    if len(events) >= 1000:
        raise HTTPException(503, "prototype buffer full")
    row = event.model_dump() | {"received_at": datetime.now(timezone.utc).isoformat()}
    events.append(row)
    return {"accepted": True, "received_at": row["received_at"]}

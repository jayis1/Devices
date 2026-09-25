"""TactileSync local FastAPI dashboard. Authored by jayis1."""
from __future__ import annotations
import json, os
from collections import deque
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="TactileSync local dashboard")
events: deque[dict] = deque(maxlen=200)

class Event(BaseModel):
    source_id: int = Field(ge=0, le=255)
    type: str
    pattern: int = Field(ge=0, le=15)
    acknowledged: bool = False

@app.get("/health")
def health() -> dict:
    return {"status": "ok", "mqtt_configured": bool(os.getenv("TACTILESYNC_MQTT_URL"))}

@app.get("/events")
def list_events() -> list[dict]:
    return list(events)

@app.post("/events", status_code=201)
def ingest(event: Event) -> dict:
    record = event.model_dump() | {"at": datetime.now(timezone.utc).isoformat()}
    events.append(record)
    return record

@app.post("/events/{index}/ack")
def acknowledge(index: int) -> dict:
    if index < 0 or index >= len(events):
        raise HTTPException(404, "event not found")
    events[index]["acknowledged"] = True
    return events[index]

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8000)

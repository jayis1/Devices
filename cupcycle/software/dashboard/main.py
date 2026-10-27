from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import os

app = FastAPI(title="CupCycle prototype API")
MAX_EVENTS = 2048
_events = []

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node: int = Field(ge=1, le=3)
    kind: int = Field(ge=1, le=3)
    sequence: int = Field(ge=0, le=65535)
    unix_s: int = Field(ge=0)
    value: int = Field(ge=0, le=65535)

@app.get("/health")
def health():
    return {"ok": True, "mqtt_configured": bool(os.getenv("MQTT_URL"))}

@app.get("/events")
def events():
    return list(_events)

@app.post("/events", status_code=202)
def ingest(event: Event):
    if len(_events) >= MAX_EVENTS:
        _events.pop(0)
    _events.append(event.model_dump())
    return {"accepted": True}

@app.get("/stock")
def stock():
    counts = [e["value"] for e in reversed(_events) if e["node"] == 2 and e["kind"] == 1]
    return {"latest_batch_count": counts[0] if counts else None, "status": "unknown" if not counts else "reported"}

# MQTT publishing is deliberately an integration hook: configure a secured client before deployment.
MQTT_URL = os.getenv("MQTT_URL")
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "cupcycle/v1/site/node/events")

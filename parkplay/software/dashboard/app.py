from datetime import datetime, timezone
from typing import Any
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="ParkPlay prototype API", version="0.1.0")
latest: dict[str, dict[str, Any]] = {}

class Reading(BaseModel):
    node_id: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9_-]+$")
    kind: str = Field(min_length=1, max_length=24)
    sequence: int = Field(ge=0, le=4294967295)
    value: int = Field(ge=-200000, le=200000)
    received_at: datetime | None = None

@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "prototype"}

@app.post("/v1/readings")
def ingest(reading: Reading) -> dict[str, str]:
    old = latest.get(reading.node_id)
    if old is not None and reading.sequence <= old["sequence"]:
        raise HTTPException(status_code=409, detail="duplicate or out-of-order sequence")
    latest[reading.node_id] = {**reading.model_dump(), "received_at": (reading.received_at or datetime.now(timezone.utc)).isoformat()}
    return {"accepted": reading.node_id}

@app.get("/v1/readings")
def readings() -> dict[str, Any]:
    return {"readings": latest}

# Deployment note: replace in-memory storage with SQLite, add authenticated MQTT
# consumer and per-site authorization before any network-facing deployment.

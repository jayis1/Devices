from datetime import datetime
from typing import Dict, Tuple
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field, model_validator

app = FastAPI(title="LibraryLoop local demo", version="0.1.0")
records: Dict[Tuple[str, int, int], dict] = {}

class Telemetry(BaseModel):
    site_id: str = Field(min_length=1, max_length=64)
    node_id: int = Field(ge=0, le=255)
    sequence: int = Field(ge=0, le=65535)
    received_at: datetime
    occupied_mask: int = Field(ge=0, le=65535)
    valid_mask: int = Field(ge=0, le=65535)
    kind: int = Field(ge=1, le=2)

    @model_validator(mode="after")
    def valid_occupancy(self):
        if self.occupied_mask & ~self.valid_mask:
            raise ValueError("occupied bits must be valid")
        return self

@app.get("/health")
def health():
    return {"status": "ok"}

@app.post("/v1/telemetry", status_code=202)
def ingest(item: Telemetry):
    key = (item.site_id, item.node_id, item.sequence)
    if key in records:
        raise HTTPException(status_code=409, detail="duplicate sequence")
    records[key] = item.model_dump(mode="json")
    return {"accepted": True}

@app.get("/v1/sites/{site_id}/status")
def status(site_id: str):
    site = [r for r in records.values() if r["site_id"] == site_id and r["kind"] == 1]
    if not site:
        return {"site_id": site_id, "occupied": None, "known_seats": 0}
    latest = site[-1]
    return {"site_id": site_id, "occupied": (latest["occupied_mask"] & latest["valid_mask"]).bit_count(), "known_seats": latest["valid_mask"].bit_count()}

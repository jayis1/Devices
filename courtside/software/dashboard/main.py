from datetime import datetime, timezone
from typing import Any
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, ConfigDict, Field

app = FastAPI(title="CourtSide Demo API", version="1.0.0")

class Observation(BaseModel):
    model_config = ConfigDict(extra="forbid")
    version: int = Field(ge=1, le=1)
    site_id: str = Field(min_length=1, max_length=48)
    node_id: str = Field(min_length=1, max_length=48)
    seq: int = Field(ge=0)
    observed_at: datetime
    kind: str = Field(pattern="^(surface_observation|inspection|maintenance_report)$")
    data: dict[str, Any]

records: list[Observation] = []
seen: set[tuple[str, int]] = set()

@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "demo", "time": datetime.now(timezone.utc).isoformat()}

@app.get("/observations")
def list_observations() -> list[dict[str, Any]]:
    return [r.model_dump(mode="json") for r in reversed(records)]

@app.post("/observations")
def ingest(item: Observation) -> dict[str, bool]:
    key = (item.node_id, item.seq)
    if key in seen:
        return {"accepted": False, "duplicate": True}
    if item.observed_at.tzinfo is None:
        raise HTTPException(status_code=422, detail="observed_at must include timezone")
    seen.add(key)
    records.append(item)
    return {"accepted": True, "duplicate": False}

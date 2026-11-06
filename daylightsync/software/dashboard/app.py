from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="DaylightSync local gateway", version="0.1.0")

class Reading(BaseModel):
    node: str = Field(pattern=r"^[0-9a-fA-F]{8}$")
    kind: str = Field(pattern=r"^(room|outdoor|shade)$")
    seq: int = Field(ge=0, le=4294967295)
    ts: int = Field(ge=0)
    data: dict

latest: dict[str, Reading] = {}

@app.post("/v1/readings", status_code=202)
def ingest(reading: Reading):
    if reading.kind == "room":
        if not isinstance(reading.data.get("lux"), (int, float)) or not 0 <= reading.data["lux"] <= 120000:
            raise HTTPException(422, "lux outside bounds")
    previous = latest.get(reading.node)
    if previous and reading.seq <= previous.seq:
        raise HTTPException(409, "duplicate or stale sequence")
    latest[reading.node] = reading
    return {"accepted": True, "received_at": datetime.now(timezone.utc).isoformat()}

@app.get("/v1/readings")
def get_readings():
    now = int(datetime.now(timezone.utc).timestamp())
    return {node: {**reading.model_dump(), "stale": now - reading.ts > 60} for node, reading in latest.items()}

@app.get("/health")
def health():
    return {"status": "ok", "mode": "local-demo"}

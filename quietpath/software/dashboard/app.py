"""QuietPath local telemetry API demonstration."""
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="QuietPath local API", version="1.0")
latest: dict[int, dict] = {}

class Telemetry(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    epoch_minutes: int = Field(ge=0)
    leq_cdb: int = Field(ge=0, le=65535)
    peak_cdb: int = Field(ge=0, le=65535)
    flags: int = Field(ge=0, le=3)

@app.get("/health")
def health():
    return {"ok": True, "component": "api"}

@app.post("/telemetry")
def post_telemetry(report: Telemetry):
    for value in (report.leq_cdb, report.peak_cdb):
        if value != 65535 and value > 14000:
            raise HTTPException(status_code=422, detail="level outside supported bounds")
    now_min = int(datetime.now(timezone.utc).timestamp() // 60)
    if abs(now_min - report.epoch_minutes) > 20:
        raise HTTPException(status_code=422, detail="timestamp outside 20-minute window")
    previous = latest.get(report.node_id)
    if previous and report.sequence == previous["sequence"]:
        raise HTTPException(status_code=409, detail="duplicate sequence")
    item = report.model_dump()
    latest[report.node_id] = item
    return {"accepted": True, "node_id": report.node_id}

@app.get("/telemetry")
def get_telemetry():
    return list(latest.values())

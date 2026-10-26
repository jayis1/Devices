from fastapi import FastAPI
from pydantic import BaseModel, Field
from typing import Literal

app = FastAPI(title="AEDReady local advisory API", version="0.1.0")

class CabinetStatus(BaseModel):
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0)
    received_uptime_s: int = Field(ge=0)
    temperature_centi_c: int
    battery_mv: int = Field(ge=0, le=65535)
    flags: int = Field(ge=0, le=65535)
    status: Literal["unknown", "check_required", "no_issue_reported"]

@app.get("/health")
def health():
    return {"status": "ok", "mode": "local-prototype"}

@app.get("/v1/cabinets")
def cabinets():
    # Persistence and BLE ingestion are intentionally not implemented.
    return {"items": [], "warning": "empty prototype; no readiness guarantee"}

@app.post("/v1/cabinets/{node_id}/status")
def ingest(node_id: int, record: CabinetStatus):
    if node_id != record.node_id:
        return {"accepted": False, "reason": "path/body node_id mismatch"}
    return {"accepted": True, "record": record.model_dump()}

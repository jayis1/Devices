# Authored by jayis1.
import os
from datetime import datetime, timezone
from typing import Literal
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="WorkshopSync local API", version="1")
TOKEN = os.environ.get("WORKSHOPSYNC_API_TOKEN", "change-before-deploy")
events: dict[tuple[int, int, int], dict] = {}
acknowledgements: list[dict] = []

class Telemetry(BaseModel):
    node_id: int = Field(ge=1)
    seq: int = Field(ge=0)
    epoch: int = Field(ge=0)
    kind: str = Field(min_length=1, max_length=48)
    quality: Literal["ok", "stale", "fault", "calibrating"]
    metrics: dict[str, float] = Field(default_factory=dict)

class Acknowledgement(BaseModel):
    card_id: str = Field(min_length=1, max_length=64)
    note: str = Field(default="", max_length=256)

def authorize(value: str | None) -> None:
    if value != f"Bearer {TOKEN}":
        raise HTTPException(status_code=401, detail="unauthorized")

@app.get("/health")
def health() -> dict:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat(), "author": "jayis1"}

@app.post("/v1/telemetry", status_code=202)
def ingest(item: Telemetry, authorization: str | None = Header(default=None)) -> dict:
    authorize(authorization)
    key = (item.node_id, item.epoch, item.seq)
    if key in events:
        raise HTTPException(status_code=409, detail="duplicate telemetry")
    events[key] = {**item.model_dump(), "received_at": datetime.now(timezone.utc).isoformat()}
    return {"accepted": True, "idempotency_key": ":".join(map(str, key))}

@app.get("/v1/cards")
def cards(authorization: str | None = Header(default=None)) -> list[dict]:
    authorize(authorization)
    return [{"id": f"{key[0]}:{key[1]}:{key[2]}", "status": "review", "evidence": event} for key, event in events.items() if event["quality"] != "ok"]

@app.post("/v1/acknowledgements", status_code=202)
def acknowledge(item: Acknowledgement, authorization: str | None = Header(default=None)) -> dict:
    authorize(authorization)
    record = {**item.model_dump(), "at": datetime.now(timezone.utc).isoformat()}
    acknowledgements.append(record)
    return {"accepted": True, "safety_notice": "Acknowledgement does not authorize tool operation."}

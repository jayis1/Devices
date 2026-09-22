# Authored by jayis1.
import os
from datetime import datetime, timezone
from typing import Literal
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="SensorySync local API", version="1")
TOKEN = os.environ.get("SENSORYSYNC_API_TOKEN", "change-before-deploy")
EVENTS: dict[tuple[int, int, int], dict] = {}
NODE_IDS = {1, 16, 32, 48, 64}
class Telemetry(BaseModel):
    node_id: int
    epoch: int = Field(ge=0)
    seq: int = Field(ge=0)
    quality: Literal["ok", "stale", "fault", "calibrating", "time_untrusted"]
    metrics: dict[str, float] = Field(default_factory=dict)
class Command(BaseModel):
    node_id: int
    kind: Literal["set_light_level", "set_fan_level", "set_noise_level", "stop_outputs"]
    value: float = Field(ge=0, le=1)
    expires_at: int = Field(ge=0)
def auth(value: str | None) -> None:
    if value != f"Bearer {TOKEN}": raise HTTPException(401, "unauthorized")
@app.get("/health")
def health() -> dict: return {"status":"ok", "author":"jayis1", "time":datetime.now(timezone.utc).isoformat()}
@app.post("/v1/telemetry", status_code=202)
def telemetry(item: Telemetry, authorization: str | None = Header(default=None)) -> dict:
    auth(authorization)
    if item.node_id not in NODE_IDS: raise HTTPException(422, "unknown node")
    key=(item.node_id,item.epoch,item.seq)
    if key in EVENTS: raise HTTPException(409, "duplicate telemetry")
    EVENTS[key]={**item.model_dump(),"received_at":datetime.now(timezone.utc).isoformat()}
    return {"accepted":True,"key":":".join(map(str,key))}
@app.get("/v1/cards")
def cards(authorization: str | None = Header(default=None)) -> list[dict]:
    auth(authorization)
    return [{"id":str(k),"status":"review","evidence":v} for k,v in EVENTS.items() if v["quality"] != "ok"]
@app.post("/v1/commands", status_code=202)
def command(item: Command, authorization: str | None = Header(default=None)) -> dict:
    auth(authorization)
    now=int(datetime.now(timezone.utc).timestamp())
    if item.node_id not in {48,64} or item.expires_at <= now or item.expires_at > now+30: raise HTTPException(422,"invalid command target or expiry")
    return {"accepted":True,"topic":f"sensorysync/v1/{item.node_id}/command","author":"jayis1"}

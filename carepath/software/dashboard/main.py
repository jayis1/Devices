from enum import Enum
from typing import Literal
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="CarePath local prototype", version="0.1.0")

class State(str, Enum):
    unknown = "UNKNOWN"
    check_required = "CHECK_REQUIRED"
    operator_checked = "OPERATOR_CHECKED"

class Observation(BaseModel):
    v: Literal[1]
    node: str = Field(min_length=1, max_length=48, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    uptime_ms: int = Field(ge=0, le=4294967295)
    kind: Literal["entry", "approach"]
    door_open: bool | None = None
    staff_check: bool | None = None
    clearance_mm: int | None = Field(default=None, ge=0, le=5000)
    valid: bool

latest: dict[str, Observation] = {}
@app.get("/health")
def health():
    return {"ok": True, "prototype": True}

@app.post("/observations")
def ingest(obs: Observation):
    if obs.kind == "entry" and (obs.door_open is None or obs.staff_check is None):
        raise HTTPException(422, "entry requires door_open and staff_check")
    if obs.kind == "approach" and obs.clearance_mm is None:
        raise HTTPException(422, "approach requires clearance_mm")
    old = latest.get(obs.node)
    if old and obs.seq <= old.seq:
        raise HTTPException(409, "duplicate or out-of-order sequence")
    latest[obs.node] = obs
    return {"accepted": True, "node": obs.node, "seq": obs.seq}

@app.get("/status")
def status():
    entries = [x for x in latest.values() if x.kind == "entry"]
    approaches = [x for x in latest.values() if x.kind == "approach"]
    if not entries or not approaches or not entries[-1].valid or not approaches[-1].valid:
        state = State.unknown
    elif entries[-1].door_open or not entries[-1].staff_check or approaches[-1].clearance_mm == 0:
        state = State.check_required
    else:
        state = State.operator_checked
    return {"state": state, "observations": len(latest), "note": "Advisory prototype; physical inspection required."}

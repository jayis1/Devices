from datetime import datetime, timezone
from enum import Enum
from typing import Dict, List
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="StepWise local prototype")
known_nodes = {"landing-a", "landing-b"}
last_sequence: Dict[str, int] = {}
reports: List[dict] = []

class Status(str, Enum):
    clear = "clear"
    needs_attention = "needs_attention"

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=32)
    sequence: int = Field(ge=0)
    status: Status
    created_at: str = Field(min_length=20, max_length=40)

def accept(event: Event):
    if event.node_id not in known_nodes:
        raise HTTPException(403, "unknown node")
    if event.sequence <= last_sequence.get(event.node_id, -1):
        raise HTTPException(409, "duplicate or stale sequence")
    last_sequence[event.node_id] = event.sequence
    report = {**event.model_dump(), "received_at": datetime.now(timezone.utc).isoformat()}
    reports.append(report)
    del reports[:-5000]
    return report

@app.get("/health")
def health():
    return {"status": "ok"}

@app.get("/reports")
def get_reports():
    return list(reversed(reports))

@app.post("/events")
def post_event(event: Event):
    return accept(event)

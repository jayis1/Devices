from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from collections import deque
from time import time

app = FastAPI(title="QueueKind local API", docs_url=None, redoc_url=None)
events = deque(maxlen=500)
class Event(BaseModel):
    node: int = Field(ge=1, le=255)
    seq: int = Field(ge=0, le=65535)
    kind: str = Field(pattern="^(arrival|complete|pause|resume)$")
    timestamp: int = Field(ge=0)

@app.get("/health")
def health():
    return {"ok": True}

@app.post("/events")
def ingest(event: Event):
    if any(e["node"] == event.node and e["seq"] == event.seq for e in events):
        raise HTTPException(status_code=409, detail="duplicate event")
    events.append({**event.model_dump(), "received": int(time())})
    return {"accepted": True}

@app.get("/queue")
def queue():
    arrivals = sum(e["kind"] == "arrival" for e in events)
    completed = sum(e["kind"] == "complete" for e in events)
    count = max(0, arrivals - completed)
    return {"count_estimate": count, "wait_band": "under-15" if count < 3 else "15-30" if count < 8 else "over-30", "advisory": True}

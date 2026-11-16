from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Literal
from collections import deque

app = FastAPI(title="ReadTogether local API", version="1.0")
events = deque(maxlen=1000)
seen = set()
class Event(BaseModel):
    v: Literal[1]
    node: str = Field(min_length=1, max_length=23, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=65535)
    ts: int = Field(ge=0)
    kind: Literal["session_start", "session_end", "book_checkin", "reminder_ack"]
    book: str | None = Field(default=None, max_length=39)
@app.get("/health")
def health(): return {"status": "ok", "mode": "local-prototype"}
@app.get("/events")
def get_events(limit: int = 100):
    if limit < 1 or limit > 1000: raise HTTPException(400, "limit must be 1..1000")
    return list(events)[-limit:]
@app.post("/events", status_code=202)
def post_event(event: Event):
    key = (event.node, event.seq)
    if key in seen: return {"accepted": False, "reason": "duplicate"}
    seen.add(key); events.append(event.model_dump()); return {"accepted": True}

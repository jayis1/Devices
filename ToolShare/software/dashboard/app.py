from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="ToolShare local inventory API", version="1")
_events: dict[tuple[int, int], dict] = {}
class Event(BaseModel):
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=4294967295)
    type: int = Field(ge=1, le=3)
    value: int = Field(ge=-2147483648, le=2147483647)
@app.get("/health")
def health(): return {"status": "ok"}
@app.get("/inventory")
def inventory(): return {"events": list(_events.values())}
@app.post("/events", status_code=201)
def post_event(event: Event):
    key = (event.node_id, event.sequence)
    if key in _events: raise HTTPException(status_code=409, detail="duplicate sequence")
    _events[key] = event.model_dump()
    return {"accepted": True}

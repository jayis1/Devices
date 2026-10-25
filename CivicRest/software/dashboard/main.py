from fastapi import FastAPI
from pydantic import BaseModel, Field
app = FastAPI(title="CivicRest prototype")
class Event(BaseModel):
    site: str = Field(min_length=1, max_length=48)
    node_id: int = Field(ge=0, le=65535)
    kind: str = Field(pattern="^(occupancy|supply|status)$")
    value: float
@app.get("/health")
def health(): return {"status":"prototype"}
@app.post("/v1/events")
def ingest(event: Event): return {"accepted": True, "event": event}

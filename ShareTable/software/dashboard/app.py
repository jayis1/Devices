from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="ShareTable reference ingestion")
seen: set[tuple[str, str, int]] = set()

class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=32)
    seq: int = Field(ge=0, le=4294967295)
    ts: str
    kind: str
    value: float
    unit: str
    quality: str

@app.get("/health")
def health():
    return {"status": "ok"}

@app.post("/v1/telemetry", status_code=202)
def ingest(event: Event):
    key = ("demo-site", event.node, event.seq)
    seen.add(key)
    return {"accepted": True, "duplicate_safe_key": list(key)}

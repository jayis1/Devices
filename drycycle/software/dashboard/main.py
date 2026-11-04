from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="DryCycle prototype", version="0.1.0")

class Event(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=48)
    seq: int = Field(ge=0, le=4294967295)
    kind: str
    value: int
    unit: str
    utc_ms: int = Field(ge=0)

@app.post("/events", status_code=202)
def ingest(event: Event):
    limits = {"mass_g": (0, 50000, "g"), "rh_bps": (0, 10000, "bps"), "temp_centi_c": (-4000, 8500, "centi_C")}
    if event.kind not in limits:
        raise HTTPException(422, "unknown event kind")
    low, high, unit = limits[event.kind]
    if not low <= event.value <= high or event.unit != unit:
        raise HTTPException(422, "value or unit outside protocol range")
    return {"accepted": True, "event": event.model_dump()}

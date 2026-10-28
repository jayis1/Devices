from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime, timezone
app = FastAPI(title="MarketChill prototype API")
records = {}
class Telemetry(BaseModel):
    node_id: str = Field(min_length=1, max_length=32, pattern=r"^[A-Za-z0-9_-]+$")
    kind: str
    sequence: int = Field(ge=0, le=65535)
    received_at: datetime
    temperature_c: float | None = Field(default=None, ge=-40, le=125)
    door_open: bool | None = None
    battery_mv: int = Field(ge=0, le=6000)
    def validate_kind(self):
        if self.kind not in ("probe", "door"):
            raise ValueError("unsupported kind")
        if self.kind == "probe" and self.temperature_c is None:
            raise ValueError("probe requires temperature")
        if self.kind == "door" and self.door_open is None:
            raise ValueError("door requires door_open")
@app.get("/health")
def health():
    return {"status": "ok", "service_time": datetime.now(timezone.utc).isoformat()}
@app.post("/v1/telemetry")
def ingest(record: Telemetry):
    try:
        record.validate_kind()
    except ValueError as exc:
        raise HTTPException(422, str(exc))
    records[record.node_id] = record.model_dump(mode="json")
    return {"accepted": True}
@app.get("/v1/telemetry/latest")
def latest():
    return list(records.values())

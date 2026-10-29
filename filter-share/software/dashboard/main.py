from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime, timezone
app = FastAPI(title="FilterShare prototype API", version="0.1")
class Snapshot(BaseModel):
    site_id: str = Field(min_length=1, max_length=48)
    node_id: str = Field(min_length=1, max_length=48)
    sequence: int = Field(ge=0, le=4294967295)
    kind: str = Field(pattern="^(pm|dock)$")
    value_a: int
    value_b: int
    received_utc: datetime | None = None
latest: dict[str, Snapshot] = {}
@app.get("/health")
def health(): return {"ok": True, "prototype": True}
@app.post("/v1/snapshot", status_code=202)
def ingest(data: Snapshot):
    key=f"{data.site_id}/{data.node_id}"
    old=latest.get(key)
    if old and old.sequence == data.sequence: raise HTTPException(409,"duplicate sequence")
    if data.kind == "pm" and (not 0 <= data.value_a <= 5000 or not 0 <= data.value_b <= 10000):
        raise HTTPException(422,"particulate value outside prototype range")
    latest[key]=data.model_copy(update={"received_utc": data.received_utc or datetime.now(timezone.utc)})
    return {"accepted": True}
@app.get("/v1/sites/{site_id}")
def site(site_id: str):
    return {"site_id": site_id, "snapshots": [v.model_dump(mode="json") for k,v in latest.items() if k.startswith(site_id+"/")]}

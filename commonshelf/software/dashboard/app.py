from pydantic import BaseModel, Field
from fastapi import FastAPI, HTTPException

app = FastAPI(title="CommonShelf prototype API")

class Reading(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=48)
    seq: int = Field(ge=0, le=4294967295)
    ts: int = Field(ge=0)
    kind: str
    value: float
    unit: str = Field(max_length=12)
    battery_mv: int = Field(ge=0, le=6000)

@app.post("/telemetry/{site}/{node}")
def ingest(site: str, node: str, reading: Reading):
    if not site or len(site) > 48 or node != reading.node:
        raise HTTPException(status_code=400, detail="site/node mismatch")
    bounds = {"mass_g": (0, 10000), "temp_c_x100": (-5000, 10000), "door": (0, 1)}
    if reading.kind not in bounds or not bounds[reading.kind][0] <= reading.value <= bounds[reading.kind][1]:
        raise HTTPException(status_code=422, detail="unsupported or out-of-range reading")
    return {"accepted": True, "site": site, "node": node, "seq": reading.seq}

@app.get("/health")
def health():
    return {"status": "development-only", "auth": "not implemented"}

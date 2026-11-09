from datetime import datetime, timezone
from fastapi import FastAPI
from pydantic import BaseModel

app = FastAPI(title="RackWise prototype", docs_url=None, redoc_url=None)

class Availability(BaseModel):
    site_id: str
    band: str
    free_bays: int | None
    updated_at: datetime
    stale: bool

@app.get("/health")
def health():
    return {"status": "ok"}

@app.get("/v1/sites/{site_id}/availability", response_model=Availability)
def availability(site_id: str):
    # Static demonstration only; MQTT aggregation adapter is intentionally not implemented.
    return Availability(site_id=site_id, band="unknown", free_bays=None,
                        updated_at=datetime.now(timezone.utc), stale=True)

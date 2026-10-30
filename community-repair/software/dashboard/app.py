"""Minimal local status API prototype."""
from fastapi import FastAPI
app = FastAPI(title="CommunityRepair", version="0.1.0")
@app.get("/health")
def health():
    return {"status": "ok"}
@app.get("/sites/{site_id}/status")
def status(site_id: str):
    return {"site_id": site_id, "status": "unknown", "reason": "no verified sensor backend configured"}
@app.get("/sites/{site_id}/events")
def events(site_id: str):
    return {"site_id": site_id, "events": []}

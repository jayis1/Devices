"""HygieneGuard FastAPI reference backend. Author: jayis1."""
import os
import time
from typing import Dict
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="HygieneGuard reference API")
TOKEN = os.getenv("HYGIENEGUARD_API_TOKEN")
stations: Dict[str, dict] = {}

class Telemetry(BaseModel):
    node_id: str = Field(pattern=r"^[a-z0-9-]{3,48}$")
    soap_g: float = Field(ge=0, le=10000)
    flow_ml: float = Field(ge=0, le=100000)
    timestamp: int = Field(ge=0)

def authorize(token: str | None) -> None:
    if TOKEN and token != TOKEN:
        raise HTTPException(status_code=401, detail="invalid token")

@app.get("/health")
def health() -> dict:
    return {"status": "ok", "known_nodes": len(stations)}

@app.post("/telemetry")
def ingest(item: Telemetry, x_api_token: str | None = Header(default=None)) -> dict:
    authorize(x_api_token)
    stations[item.node_id] = item.model_dump()
    return {"accepted": True, "node_id": item.node_id}

@app.get("/stations")
def get_stations() -> list[dict]:
    now = int(time.time())
    return [{**v, "state": "stale" if now - v["timestamp"] > 300 else "current"} for v in stations.values()]

@app.get("/cards")
def cards() -> list[dict]:
    return [{"node_id": k, "kind": "refill"} for k, v in stations.items() if v["soap_g"] < 100]

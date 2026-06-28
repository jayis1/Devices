"""PestSync Backend — Telemetry Router"""
from fastapi import APIRouter, Query
from pydantic import BaseModel
from datetime import datetime, timezone
from typing import Optional

router = APIRouter()


class TelemetryPoint(BaseModel):
    device_id: str
    timestamp: datetime
    temperature_c: Optional[float] = None
    humidity_pct: Optional[float] = None
    pressure_hpa: Optional[float] = None
    battery_pct: Optional[int] = None


@router.get("/{device_id}")
async def get_telemetry(
    device_id: str,
    start: datetime = Query(...),
    end: datetime = Query(default_factory=lambda: datetime.now(timezone.utc)),
    interval: str = Query("1m", regex="^(1m|5m|1h|1d)$"),
):
    """Get time-series telemetry for a device."""
    # In production: query TimescaleDB hypertable
    return {
        "device_id": device_id,
        "start": start.isoformat(),
        "end": end.isoformat(),
        "interval": interval,
        "points": [],
    }


@router.post("/")
async def ingest_telemetry(point: TelemetryPoint):
    """Ingest a telemetry data point (from MQTT bridge or direct API)."""
    return {"status": "ok", "device_id": point.device_id, "timestamp": point.timestamp}
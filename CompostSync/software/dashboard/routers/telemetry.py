"""Telemetry router — time series data ingestion and retrieval."""
from datetime import datetime, timedelta
from fastapi import APIRouter, Depends, Query
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession

from db import get_db
from models.schemas import TelemetryResponse, TelemetryRecord

router = APIRouter()


@router.post("/")
async def ingest_telemetry(device_id: str, node_id: str, data: dict,
                            db: AsyncSession = Depends(get_db)):
    """Ingest telemetry data point (called by MQTT handler or directly)."""
    record = TelemetryRecord(
        device_id=device_id,
        node_id=node_id,
        uptime_s=data.get("uptime", 0),
        battery_pct=data.get("batt", 0),
        temp_c=data.get("temps", []),
        moisture_pct=data.get("moisture", []),
        co2_ppm=data.get("co2", 0),
        methane_ppm=data.get("ch4", 0),
        mass_grams=data.get("mass", 0),
        ph=data.get("ph"),
        vent_position=data.get("vent", 0),
        phase=data.get("phase", "unknown"),
        alerts=data.get("alerts", 0),
    )
    db.add(record)
    await db.commit()
    return {"status": "ok", "timestamp": datetime.utcnow()}


@router.get("/{device_id}", response_model=list[TelemetryResponse])
async def get_telemetry(
    device_id: str,
    hours: int = Query(24, ge=1, le=720),
    db: AsyncSession = Depends(get_db),
):
    """Get telemetry for a device over the last N hours."""
    since = datetime.utcnow() - timedelta(hours=hours)
    result = await db.execute(
        select(TelemetryRecord)
        .where(TelemetryRecord.device_id == device_id)
        .where(TelemetryRecord.timestamp >= since)
        .order_by(TelemetryRecord.timestamp.desc())
        .limit(500)
    )
    records = result.scalars().all()
    return [
        TelemetryResponse(
            timestamp=r.timestamp, node_id=r.node_id,
            temp_c=r.temp_c or [], moisture_pct=r.moisture_pct or [],
            co2_ppm=r.co2_ppm, methane_ppm=r.methane_ppm,
            mass_grams=r.mass_grams, ph=r.ph,
            vent_position=r.vent_position, phase=r.phase,
            alerts=r.alerts
        )
        for r in records
    ]


@router.get("/{device_id}/latest")
async def get_latest_telemetry(device_id: str, db: AsyncSession = Depends(get_db)):
    """Get the most recent telemetry reading."""
    result = await db.execute(
        select(TelemetryRecord)
        .where(TelemetryRecord.device_id == device_id)
        .order_by(desc(TelemetryRecord.timestamp))
        .limit(1)
    )
    record = result.scalar_one_or_none()
    if not record:
        return {"error": "no data"}
    return {
        "timestamp": record.timestamp,
        "temp_c": record.temp_c,
        "moisture_pct": record.moisture_pct,
        "co2_ppm": record.co2_ppm,
        "methane_ppm": record.methane_ppm,
        "mass_grams": record.mass_grams,
        "ph": record.ph,
        "phase": record.phase,
        "alerts": record.alerts,
        "battery_pct": record.battery_pct,
    }
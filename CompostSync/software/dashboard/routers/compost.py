"""Compost router — compost status, maturity, recommendations, recipes."""
from datetime import datetime, timedelta
from fastapi import APIRouter, Depends
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from db import get_db
from models.schemas import CompostStatusResponse, CompostCycle

router = APIRouter()


@router.get("/{device_id}/status", response_model=CompostStatusResponse)
async def get_compost_status(device_id: str, db: AsyncSession = Depends(get_db)):
    """Get current compost status with ML predictions."""
    # Get latest telemetry
    from models.schemas import TelemetryRecord
    result = await db.execute(
        select(TelemetryRecord)
        .where(TelemetryRecord.device_id == device_id)
        .order_by(TelemetryRecord.timestamp.desc())
        .limit(1)
    )
    latest = result.scalar_one_or_none()

    if not latest:
        return CompostStatusResponse(
            device_id=device_id, phase="unknown", maturity_score=0,
            cn_ratio=30, days_to_ready=90, recommendation="No data yet",
            mass_kg=0, diverted_kg=0
        )

    temps = latest.temp_c or [0, 0, 0]
    temp = temps[1] if len(temps) > 1 else temps[0] if temps else 0

    # Heuristic (replace with ML inference)
    maturity = 0.0
    if latest.co2_ppm < 300 and latest.methane_ppm < 50:
        maturity = 95.0
    elif latest.co2_ppm < 800:
        maturity = 70.0
    elif temp > 50:
        maturity = 20.0

    phase = latest.phase or "unknown"

    # Get compost cycle
    cycle_result = await db.execute(
        select(CompostCycle)
        .where(CompostCycle.device_id == device_id)
        .order_by(CompostCycle.start_date.desc())
        .limit(1)
    )
    cycle = cycle_result.scalar_one_or_none()

    diverted = cycle.total_diverted_kg if cycle else 0.0
    mass_kg = (latest.mass_grams or 0) / 1000.0

    recommendation = "Add materials to start composting!"
    if latest.methane_ppm > 1000:
        recommendation = "🚨 TURN PILE NOW — anaerobic conditions detected!"
    elif latest.phase == "thermophilic":
        recommendation = f"🔥 Hot composting at {temp}°C! Turn in 3-5 days."
    elif latest.phase == "cured":
        recommendation = "🎉 Your compost is ready to harvest!"

    return CompostStatusResponse(
        device_id=device_id, phase=phase, maturity_score=maturity,
        cn_ratio=30, days_to_ready=max(0, int(90 - maturity * 0.9)),
        recommendation=recommendation, mass_kg=mass_kg,
        diverted_kg=diverted
    )


@router.get("/{device_id}/timeline")
async def get_timeline(device_id: str, days: int = 30, db: AsyncSession = Depends(get_db)):
    """Get compost timeline data for charts."""
    from models.schemas import TelemetryRecord
    since = datetime.utcnow() - timedelta(days=days)
    result = await db.execute(
        select(TelemetryRecord)
        .where(TelemetryRecord.device_id == device_id)
        .where(TelemetryRecord.timestamp >= since)
        .order_by(TelemetryRecord.timestamp)
    )
    records = result.scalars().all()
    return [
        {
            "timestamp": r.timestamp.isoformat(),
            "temp_c": r.temp_c,
            "co2_ppm": r.co2_ppm,
            "moisture": r.moisture_pct,
            "mass_g": r.mass_grams,
            "phase": r.phase,
        }
        for r in records
    ]


@router.get("/{device_id}/recipes")
async def get_recipes(device_id: str, db: AsyncSession = Depends(get_db)):
    """Get composting recipe suggestions based on current conditions."""
    from models.schemas import TelemetryRecord
    result = await db.execute(
        select(TelemetryRecord)
        .where(TelemetryRecord.device_id == device_id)
        .order_by(TelemetryRecord.timestamp.desc())
        .limit(1)
    )
    latest = result.scalar_one_or_none()

    if not latest:
        return {"recipes": []}

    recipes = []
    moisture = (latest.moisture_pct or [50])[0]
    temp = (latest.temp_c or [20, 20, 20])[1] / 10.0

    if moisture < 40:
        recipes.append({
            "name": "Add moisture",
            "ingredients": ["Water (2-3 liters)", "Green materials (vegetable scraps, coffee grounds)"],
            "reason": f"Moisture is {moisture}%, optimal is 50-60%",
            "c_ratio": 0,
        })
    if moisture > 65:
        recipes.append({
            "name": "Add dry browns",
            "ingredients": ["Shredded cardboard", "Dry leaves", "Sawdust", "Paper towels"],
            "reason": f"Moisture is {moisture}%, pile is too wet",
            "c_ratio": 60,
        })
    if temp < 30:
        recipes.append({
            "name": "Boost nitrogen",
            "ingredients": ["Coffee grounds", "Grass clippings", "Vegetable scraps", "Manure"],
            "reason": f"Pile is only {temp:.1f}°C, needs more nitrogen to heat up",
            "c_ratio": 15,
        })
    if temp > 65:
        recipes.append({
            "name": "Cool it down",
            "ingredients": ["Dry browns", "Turn pile to release heat"],
            "reason": f"Pile is {temp:.1f}°C, may be too hot for microbes",
            "c_ratio": 50,
        })

    if not recipes:
        recipes.append({
            "name": "All good!",
            "ingredients": ["Keep adding 2:1 brown:green mix"],
            "reason": "Conditions are optimal",
            "c_ratio": 30,
        })

    return {"recipes": recipes}
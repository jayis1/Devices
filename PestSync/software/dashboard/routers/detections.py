"""PestSync Backend — Detections Router"""
from fastapi import APIRouter, Query
from pydantic import BaseModel
from datetime import datetime, timezone
from typing import Optional

router = APIRouter()

# Pest class names (matches firmware psp_protocol.h)
PEST_NAMES = {
    0: "House Mouse", 1: "Norway Rat", 2: "German Cockroach", 3: "American Cockroach",
    4: "Argentine Ant", 5: "Carpenter Ant", 6: "Mosquito", 7: "House Fly",
    8: "Fruit Fly", 9: "Bedbug", 10: "Termite (worker)", 11: "Termite (swarmer)",
    12: "Spider", 13: "Silverfish", 14: "Carpet Beetle", 255: "None",
}


class DetectionEvent(BaseModel):
    id: int | None = None
    device_id: str
    timestamp: datetime | None = None
    pest_class: int
    pest_name: str
    confidence: float
    count: int = 1
    thermal_max_c: Optional[float] = None
    ir_illumination: bool = False
    alerts: int = 0


@router.get("/", response_model=list[DetectionEvent])
async def list_detections(
    device_id: Optional[str] = None,
    pest_class: Optional[int] = None,
    start: datetime = Query(default=None),
    end: datetime = Query(default=None),
    limit: int = Query(100, le=1000),
):
    """List pest detection events."""
    # In production: query detections table with filters
    return [
        DetectionEvent(
            id=1, device_id="0x0010",
            timestamp=datetime.now(timezone.utc),
            pest_class=0, pest_name="House Mouse",
            confidence=0.82, count=1, thermal_max_c=33.5,
            ir_illumination=True, alerts=1,
        ),
        DetectionEvent(
            id=2, device_id="0x0010",
            timestamp=datetime.now(timezone.utc),
            pest_class=2, pest_name="German Cockroach",
            confidence=0.71, count=3, thermal_max_c=22.0,
            ir_illumination=True, alerts=1,
        ),
    ]


@router.post("/", response_model=DetectionEvent)
async def create_detection(event: DetectionEvent):
    """Record a new pest detection event."""
    event.id = hash(event.device_id + str(event.timestamp)) % 1000000
    event.pest_name = PEST_NAMES.get(event.pest_class, "Unknown")
    return event


@router.get("/heatmap")
async def get_heatmap(device_id: str | None = None):
    """Get 24-hour activity heatmap (detections per hour)."""
    # In production: aggregate from detections table
    return {
        "device_id": device_id,
        "hours": list(range(24)),
        "counts": [0, 0, 1, 2, 3, 1, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 2, 1],
        "total": 16,
        "peak_hour": 4,
        "pattern": "nocturnal",
    }


@router.get("/stats")
async def get_stats(days: int = Query(30, le=365)):
    """Get detection statistics for the last N days."""
    return {
        "days": days,
        "total_detections": 142,
        "by_species": {
            "House Mouse": 45,
            "German Cockroach": 67,
            "Norway Rat": 3,
            "Spider": 15,
            "Silverfish": 12,
        },
        "by_zone": {
            "Kitchen": 78,
            "Garage": 42,
            "Attic": 22,
        },
        "trend": "increasing",  # increasing, stable, decreasing
    }
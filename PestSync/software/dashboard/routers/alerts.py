"""PestSync Backend — Alerts Router"""
from fastapi import APIRouter, Query
from pydantic import BaseModel
from datetime import datetime, timezone
from typing import Optional

router = APIRouter()


class AlertModel(BaseModel):
    id: int
    device_id: Optional[str] = None
    timestamp: datetime
    alert_type: str
    severity: int  # 0=info, 1=warning, 2=critical
    title: str
    message: str
    is_read: bool = False


@router.get("/", response_model=list[AlertModel])
async def list_alerts(
    unread_only: bool = False,
    severity: Optional[int] = None,
    limit: int = Query(50, le=200),
):
    """List alerts for the user."""
    alerts = [
        AlertModel(
            id=1, device_id="0x0010",
            timestamp=datetime.now(timezone.utc),
            alert_type="pest_detected", severity=1,
            title="German Cockroach detected",
            message="Cockroach detected in kitchen at 9:14 PM. Confidence: 71%.",
            is_read=False,
        ),
        AlertModel(
            id=2, device_id="0x0021",
            timestamp=datetime.now(timezone.utc),
            alert_type="trap_triggered", severity=0,
            title="Kitchen trap triggered",
            message="Snap trap caught a mouse (22g). Check and reset.",
            is_read=False,
        ),
        AlertModel(
            id=3, device_id="0x0010",
            timestamp=datetime.now(timezone.utc),
            alert_type="infestation_risk", severity=2,
            title="⚠️ High infestation risk: Cockroaches",
            message="Based on 14-day activity, cockroach infestation likely within 10 days. "
                    "Recommended: set 3 traps, activate ultrasonic 8PM-6AM.",
            is_read=False,
        ),
    ]

    if unread_only:
        alerts = [a for a in alerts if not a.is_read]
    if severity is not None:
        alerts = [a for a in alerts if a.severity == severity]

    return alerts[:limit]


@router.post("/{alert_id}/read")
async def mark_read(alert_id: int):
    """Mark an alert as read."""
    return {"status": "ok", "alert_id": alert_id, "read": True}


@router.post("/read-all")
async def mark_all_read():
    """Mark all alerts as read."""
    return {"status": "ok", "marked": "all"}


@router.delete("/{alert_id}")
async def delete_alert(alert_id: int):
    """Delete an alert."""
    return {"status": "deleted", "alert_id": alert_id}
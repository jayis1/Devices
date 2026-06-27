"""Alerts router — manage and acknowledge alerts."""
from datetime import datetime, timedelta
from fastapi import APIRouter, Depends, Query
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession

from db import get_db
from models.schemas import Alert, AlertResponse

router = APIRouter()


@router.get("/{device_id}", response_model=list[AlertResponse])
async def get_alerts(
    device_id: str,
    hours: int = Query(72, ge=1, le=720),
    db: AsyncSession = Depends(get_db),
):
    """Get alerts for a device in the last N hours."""
    since = datetime.utcnow() - timedelta(hours=hours)
    result = await db.execute(
        select(Alert)
        .where(Alert.device_id == device_id)
        .where(Alert.timestamp >= since)
        .order_by(desc(Alert.timestamp))
        .limit(100)
    )
    alerts = result.scalars().all()
    return [
        AlertResponse(
            id=a.id, alert_type=a.alert_type, severity=a.severity,
            message=a.message, timestamp=a.timestamp, acknowledged=a.acknowledged
        )
        for a in alerts
    ]


@router.put("/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: int, db: AsyncSession = Depends(get_db)):
    """Acknowledge an alert."""
    result = await db.execute(select(Alert).where(Alert.id == alert_id))
    alert = result.scalar_one_or_none()
    if not alert:
        from fastapi import HTTPException
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.acknowledged = True
    await db.commit()
    return {"status": "acknowledged", "alert_id": alert_id}
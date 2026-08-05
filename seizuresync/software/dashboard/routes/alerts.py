"""SeizureSync — Alert routes."""
from fastapi import APIRouter

router = APIRouter()


@router.get("")
async def list_alerts(patient_id: str, limit: int = 50):
    """List alert history for a patient."""
    return []


@router.post("/{alert_id}/ack")
async def ack_alert(patient_id: str, alert_id: str):
    """Acknowledge an alert (from mobile app)."""
    return {"status": "acked", "alert_id": alert_id}
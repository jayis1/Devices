"""PestSync Backend — Deterrents Router"""
from fastapi import APIRouter
from pydantic import BaseModel
from datetime import datetime, timezone
from typing import Optional

router = APIRouter()


class DeterrentStatus(BaseModel):
    device_id: str
    name: str
    mode: str  # off, schedule, adaptive, always_on
    band: str  # rodent, insect, both
    ultrasonic_active: bool
    strobe_active: bool
    diffuser_active: bool
    oil_level: int
    total_ultrasonic_s: int
    diffuser_doses: int
    battery_pct: int


class DeterrentCommand(BaseModel):
    mode: str  # off, schedule, adaptive, always_on
    band: Optional[str] = "both"  # rodent, insect, both
    duration_s: Optional[int] = 300


@router.get("/", response_model=list[DeterrentStatus])
async def list_deterrents():
    """List all deterrent nodes."""
    return [
        DeterrentStatus(
            device_id="0x0030", name="Kitchen Deterrent", mode="adaptive",
            band="both", ultrasonic_active=True, strobe_active=False,
            diffuser_active=False, oil_level=75, total_ultrasonic_s=14400,
            diffuser_doses=12, battery_pct=85,
        ),
        DeterrentStatus(
            device_id="0x0031", name="Garage Deterrent", mode="schedule",
            band="rodent", ultrasonic_active=False, strobe_active=False,
            diffuser_active=False, oil_level=45, total_ultrasonic_s=28800,
            diffuser_doses=24, battery_pct=72,
        ),
    ]


@router.get("/{device_id}", response_model=DeterrentStatus)
async def get_deterrent(device_id: str):
    """Get a deterrent node's status."""
    return DeterrentStatus(
        device_id=device_id, name=f"Deterrent {device_id}", mode="adaptive",
        band="both", ultrasonic_active=True, strobe_active=False,
        diffuser_active=False, oil_level=75, total_ultrasonic_s=14400,
        diffuser_doses=12, battery_pct=85,
    )


@router.post("/{device_id}/command")
async def send_command(device_id: str, cmd: DeterrentCommand):
    """Send a command to a deterrent node."""
    # In production: publish via MQTT to pestsync/{user_id}/{device_id}/command
    return {
        "status": "sent",
        "device_id": device_id,
        "command": cmd.model_dump(),
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@router.post("/{device_id}/strobe")
async def trigger_strobe(device_id: str):
    """Trigger an immediate strobe burst."""
    return {"status": "sent", "device_id": device_id, "action": "strobe"}


@router.post("/{device_id}/diffuse")
async def trigger_diffuse(device_id: str):
    """Trigger an immediate essential oil dose."""
    return {"status": "sent", "device_id": device_id, "action": "diffuse"}


@router.get("/{device_id}/effectiveness")
async def effectiveness(device_id: str, days: int = 7):
    """Get deterrent effectiveness metrics."""
    return {
        "device_id": device_id,
        "days": days,
        "pre_activity_count": 45,
        "post_activity_count": 12,
        "reduction_pct": 73.3,
        "effectiveness_score": 0.73,
        "verdict": "effective",
    }
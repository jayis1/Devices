"""PestSync Backend — Traps Router"""
from fastapi import APIRouter, Query
from pydantic import BaseModel
from datetime import datetime, timezone
from typing import Optional

router = APIRouter()


class TrapStatus(BaseModel):
    device_id: str
    name: str
    status: str  # armed, triggered, needs_reset, tampered
    catch_weight_g: int = 0
    catch_class: Optional[str] = None
    bait_level: int = 100
    battery_pct: int = 100
    last_triggered: Optional[datetime] = None


CATCH_CLASSES = {0: "mouse", 1: "rat", 2: "insect", 3: "false_trigger", 255: "unknown"}


@router.get("/", response_model=list[TrapStatus])
async def list_traps():
    """List all smart traps and their current status."""
    return [
        TrapStatus(
            device_id="0x0020", name="Garage Trap", status="armed",
            catch_weight_g=0, bait_level=85, battery_pct=92,
        ),
        TrapStatus(
            device_id="0x0021", name="Kitchen Trap", status="triggered",
            catch_weight_g=22, catch_class="mouse", bait_level=60,
            battery_pct=88, last_triggered=datetime.now(timezone.utc),
        ),
        TrapStatus(
            device_id="0x0022", name="Attic Trap", status="needs_reset",
            catch_weight_g=180, catch_class="rat", bait_level=40,
            battery_pct=76, last_triggered=datetime.now(timezone.utc),
        ),
    ]


@router.get("/{device_id}", response_model=TrapStatus)
async def get_trap(device_id: str):
    """Get a specific trap's status."""
    return TrapStatus(
        device_id=device_id, name=f"Trap {device_id}", status="armed",
        catch_weight_g=0, bait_level=85, battery_pct=92,
    )


@router.post("/{device_id}/reset")
async def reset_trap(device_id: str):
    """Send reset/rearm command to a trap."""
    # In production: publish command via MQTT
    return {"status": "ok", "device_id": device_id, "action": "reset"}


@router.get("/{device_id}/history")
async def trap_history(
    device_id: str,
    start: datetime = Query(default=None),
    end: datetime = Query(default=None),
    limit: int = Query(50, le=500),
):
    """Get catch history for a trap."""
    return {
        "device_id": device_id,
        "events": [
            {
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "catch_class": "mouse",
                "weight_g": 22,
            },
        ],
        "total_catches": 1,
    }
"""PestSync Backend — Devices Router"""
from fastapi import APIRouter
from pydantic import BaseModel
from typing import Optional
from datetime import datetime, timezone

router = APIRouter()


class DeviceCreate(BaseModel):
    id: str
    name: str
    node_type: str  # hub, sentinel, trap, deterrent
    firmware_version: Optional[str] = "1.0.0"


class DeviceResponse(BaseModel):
    id: str
    name: str
    node_type: str
    firmware_version: str
    last_seen: Optional[datetime]
    is_active: bool


@router.get("/", response_model=list[DeviceResponse])
async def list_devices():
    """List all registered devices for the authenticated user."""
    return [
        DeviceResponse(
            id="0x0001", name="Living Room Hub", node_type="hub",
            firmware_version="1.0.0", last_seen=datetime.now(timezone.utc),
            is_active=True
        ),
        DeviceResponse(
            id="0x0010", name="Kitchen Sentinel", node_type="sentinel",
            firmware_version="1.0.0", last_seen=datetime.now(timezone.utc),
            is_active=True
        ),
        DeviceResponse(
            id="0x0020", name="Garage Trap", node_type="trap",
            firmware_version="1.0.0", last_seen=datetime.now(timezone.utc),
            is_active=True
        ),
    ]


@router.post("/", response_model=DeviceResponse)
async def register_device(device: DeviceCreate):
    """Register a new device."""
    return DeviceResponse(
        id=device.id, name=device.name, node_type=device.node_type,
        firmware_version=device.firmware_version,
        last_seen=datetime.now(timezone.utc), is_active=True
    )


@router.get("/{device_id}", response_model=DeviceResponse)
async def get_device(device_id: str):
    """Get device details."""
    return DeviceResponse(
        id=device_id, name=f"Device {device_id}", node_type="sentinel",
        firmware_version="1.0.0", last_seen=datetime.now(timezone.utc),
        is_active=True
    )


@router.delete("/{device_id}")
async def delete_device(device_id: str):
    """Remove a device."""
    return {"status": "deleted", "device_id": device_id}
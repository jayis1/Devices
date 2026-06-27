"""Devices router — manage compost devices (hubs, nodes)."""
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from db import get_db
from models.schemas import DeviceCreate, DeviceResponse

router = APIRouter()


@router.post("/", response_model=DeviceResponse)
async def create_device(device: DeviceCreate, db: AsyncSession = Depends(get_db)):
    """Register a new compost device/hub."""
    from models.schemas import Device
    db_device = Device(
        id=device.id,
        name=device.name,
        bin_volume_liters=device.bin_volume_liters,
        compost_type=device.compost_type,
        last_seen=datetime.utcnow(),
    )
    db.add(db_device)
    await db.commit()
    await db.refresh(db_device)
    return DeviceResponse(
        id=db_device.id, name=db_device.name,
        bin_volume_liters=db_device.bin_volume_liters,
        compost_type=db_device.compost_type, last_seen=db_device.last_seen
    )


@router.get("/{device_id}", response_model=DeviceResponse)
async def get_device(device_id: str, db: AsyncSession = Depends(get_db)):
    from models.schemas import Device
    result = await db.execute(select(Device).where(Device.id == device_id))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return DeviceResponse(
        id=device.id, name=device.name,
        bin_volume_liters=device.bin_volume_liters,
        compost_type=device.compost_type, last_seen=device.last_seen
    )


@router.get("/")
async def list_devices(db: AsyncSession = Depends(get_db)):
    from models.schemas import Device
    result = await db.execute(select(Device))
    devices = result.scalars().all()
    return [{"id": d.id, "name": d.name, "last_seen": d.last_seen} for d in devices]


@router.delete("/{device_id}")
async def delete_device(device_id: str, db: AsyncSession = Depends(get_db)):
    from models.schemas import Device
    result = await db.execute(select(Device).where(Device.id == device_id))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    await db.delete(device)
    await db.commit()
    return {"status": "deleted", "device_id": device_id}
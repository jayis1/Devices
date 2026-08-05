"""SeizureSync — Seizure event routes."""
from fastapi import APIRouter
from models import EventCreate, EventOut

router = APIRouter()


@router.get("", response_model=list[EventOut])
async def list_events(patient_id: str, limit: int = 50):
    """List seizure events for a patient."""
    # Production: query from TimescaleDB
    return []


@router.post("", response_model=EventOut, status_code=201)
async def create_event(patient_id: str, ev: EventCreate):
    """Create a seizure event (manual entry or device-triggered)."""
    # Production: insert into TimescaleDB, trigger ML inference
    return EventOut(id=0, patient_id=patient_id, **ev.dict(),
                    triggers=ev.triggers or [])


@router.get("/{event_id}", response_model=EventOut)
async def get_event(patient_id: str, event_id: int):
    """Get a specific seizure event."""
    from fastapi import HTTPException
    raise HTTPException(404, "Event not found")
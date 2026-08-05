"""SeizureSync — Patient routes."""
from fastapi import APIRouter, HTTPException
from models import PatientCreate, PatientOut, Base
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
import uuid

router = APIRouter()

DATABASE_URL = "postgresql://seizuresync:seizuresync@localhost/seizuresync"
engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(bind=engine)


@router.get("", response_model=list[PatientOut])
async def list_patients():
    db = SessionLocal()
    try:
        patients = db.query(Base.classes.patients).all()
        return [PatientOut(**p.__dict__) for p in patients]
    finally:
        db.close()


@router.post("", response_model=PatientOut, status_code=201)
async def create_patient(p: PatientCreate):
    db = SessionLocal()
    try:
        pid = str(uuid.uuid4())
        # Production: insert via ORM
        return PatientOut(id=pid, name=p.name,
                          epilepsy_type=p.epilepsy_type,
                          seizure_frequency=p.seizure_frequency or 0.0,
                          sudep_risk_score=0.0)
    finally:
        db.close()


@router.get("/{patient_id}", response_model=PatientOut)
async def get_patient(patient_id: str):
    db = SessionLocal()
    try:
        p = db.query(Base.classes.patients).filter_by(id=patient_id).first()
        if not p:
            raise HTTPException(404, "Patient not found")
        return PatientOut(**p.__dict__)
    finally:
        db.close()
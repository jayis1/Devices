"""SeizureSync — Neurologist report routes."""
from fastapi import APIRouter
from fastapi.responses import StreamingResponse
from reports import generate_neurologist_report
import io

router = APIRouter()


@router.post("")
async def generate_report(patient_id: str):
    """Generate a neurologist PDF report."""
    # Production: gather events, risk, SUDEP, triggers from DB
    patient = {"name": "Test Patient"}
    events = []
    risk = {"risk_24h": 12.5, "risk_7d": 45.0}
    sudep = {"annual_risk_pct": 0.3, "apnea_density": 2.1, "prone_episodes": 1}
    triggers = {"sleep_deprivation": 0.42, "missed_medication": 0.31}
    pdf = generate_neurologist_report(patient, events, risk, sudep, triggers)
    return StreamingResponse(io.BytesIO(pdf), media_type="application/pdf",
                              headers={"Content-Disposition":
                                       f"attachment; filename=report_{patient_id}.pdf"})


@router.get("/{report_id}")
async def get_report(patient_id: str, report_id: str):
    """Download a previously generated report."""
    from fastapi import HTTPException
    raise HTTPException(404, "Report not found")
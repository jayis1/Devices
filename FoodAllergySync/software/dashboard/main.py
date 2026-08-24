from __future__ import annotations

from contextlib import asynccontextmanager
from typing import List

from fastapi import FastAPI
from pydantic import BaseModel

from ml_inference import assess_label, summarize_strip_ratio


class ProfileIn(BaseModel):
    name: str
    allergens: List[str]
    severity: str = "medium"
    carry_required: bool = True


class PackageScanIn(BaseModel):
    text: str
    household_allergens: List[str]


class StripScanIn(BaseModel):
    ratio: float


class EpiReadinessIn(BaseModel):
    present: bool
    temperature_c: float
    days_to_expiry: int


STATE = {
    "profiles": [],
    "alerts": [],
}


@asynccontextmanager
async def lifespan(app: FastAPI):
    yield


app = FastAPI(title="FoodAllergySync Dashboard", version="0.1.0", lifespan=lifespan)


@app.get("/health")
def health() -> dict:
    return {"status": "ok", "service": "foodallergysync-dashboard"}


@app.get("/profiles")
def get_profiles() -> list[dict]:
    return STATE["profiles"]


@app.post("/profiles")
def create_profile(profile: ProfileIn) -> dict:
    record = profile.model_dump()
    STATE["profiles"].append(record)
    return {"ok": True, "profile": record}


@app.post("/scan/package")
def scan_package(payload: PackageScanIn) -> dict:
    assessment = assess_label(payload.text, payload.household_allergens)
    if assessment.risk_level != "safe":
        STATE["alerts"].append({"type": "package", "risk": assessment.risk_level, "allergens": assessment.allergens})
    return {
        "risk_level": assessment.risk_level,
        "allergens": assessment.allergens,
        "rationale": assessment.rationale,
    }


@app.post("/scan/strip")
def scan_strip(payload: StripScanIn) -> dict:
    classification = summarize_strip_ratio(payload.ratio)
    if classification in {"trace", "positive", "invalid"}:
        STATE["alerts"].append({"type": "strip", "classification": classification})
    return {"classification": classification, "ratio": payload.ratio}


@app.post("/readiness/epipen")
def readiness(payload: EpiReadinessIn) -> dict:
    ready = payload.present and 2.0 < payload.temperature_c < 30.0 and payload.days_to_expiry > 14
    if not ready:
        STATE["alerts"].append({"type": "epipen", "ready": False})
    return {"ready": ready}


@app.get("/alerts/active")
def active_alerts() -> list[dict]:
    return STATE["alerts"]


@app.get("/dashboard/summary")
def dashboard_summary() -> dict:
    readiness_score = max(0, 100 - 10 * len(STATE["alerts"]))
    return {
        "readiness_score": readiness_score,
        "active_alerts": len(STATE["alerts"]),
        "meal_safety_score": max(0, 100 - 12 * len([a for a in STATE["alerts"] if a["type"] == "package"])),
        "carry_compliance_score": max(0, 100 - 20 * len([a for a in STATE["alerts"] if a["type"] == "epipen"])),
        "temperature_integrity_score": max(0, 100 - 15 * len([a for a in STATE["alerts"] if a["type"] == "strip"])),
    }

"""
RehabSync — Cloud Dashboard Backend (FastAPI)

Provides REST API for exercise sessions, exercise plans, patient
management, recovery forecasting, adherence metrics, therapist
dashboard, clinical reports, and OTA firmware distribution.
Receives telemetry from Rehab Hub via MQTT.

Run: uvicorn main:app --host 0.0.0.0 --port 8000
"""
from __future__ import annotations

import asyncio
import json
import os
from datetime import datetime, timezone
from typing import Optional

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

app = FastAPI(
    title="RehabSync API",
    version="1.0.0",
    description="AI-powered physical therapy & post-surgery rehabilitation system — cloud backend",
)

# === MQTT Configuration ===
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_TELEMETRY = "rehab-sync/telemetry/#"
MQTT_TOPIC_SESSIONS = "rehab-sync/session/#"
MQTT_TOPIC_ALERTS = "rehab-sync/alerts/#"

# === In-memory stores (production: PostgreSQL + InfluxDB) ===
patients: dict[str, dict] = {}
therapists: dict[str, dict] = {}
exercise_plans: dict[str, dict] = {}
sessions: dict[str, dict] = {}
telemetry_store: list[dict] = []
alerts: list[dict] = []
recovery_forecasts: dict[str, dict] = {}
adherence_data: dict[str, dict] = {}
form_trends: dict[str, list] = {}
rom_progress: dict[str, list] = {}
websocket_clients: list[WebSocket] = []

# === Exercise catalog (30 exercises) ===
EXERCISES = [
    {"id": 1, "name": "Squat", "category": "lower", "target_joints": ["knee", "hip"]},
    {"id": 2, "name": "Lunge", "category": "lower", "target_joints": ["knee", "hip", "ankle"]},
    {"id": 3, "name": "Straight Leg Raise", "category": "lower", "target_joints": ["hip"]},
    {"id": 4, "name": "Knee Extension", "category": "lower", "target_joints": ["knee"]},
    {"id": 5, "name": "Hip Abduction", "category": "lower", "target_joints": ["hip"]},
    {"id": 6, "name": "Shoulder Flexion", "category": "upper", "target_joints": ["shoulder"]},
    {"id": 7, "name": "Shoulder Abduction", "category": "upper", "target_joints": ["shoulder"]},
    {"id": 8, "name": "Bicep Curl", "category": "upper", "target_joints": ["elbow"]},
    {"id": 9, "name": "Tricep Extension", "category": "upper", "target_joints": ["elbow"]},
    {"id": 10, "name": "External Rotation", "category": "upper", "target_joints": ["shoulder"]},
    {"id": 11, "name": "Wall Push-up", "category": "upper", "target_joints": ["shoulder", "elbow"]},
    {"id": 12, "name": "Sit to Stand", "category": "functional", "target_joints": ["knee", "hip"]},
    {"id": 13, "name": "Single Leg Stance", "category": "balance", "target_joints": ["ankle", "hip"]},
    {"id": 14, "name": "Heel Raise", "category": "lower", "target_joints": ["ankle"]},
    {"id": 15, "name": "Step Up", "category": "functional", "target_joints": ["knee", "hip"]},
    {"id": 16, "name": "Glute Bridge", "category": "lower", "target_joints": ["hip", "knee"]},
    {"id": 17, "name": "Clamshell", "category": "lower", "target_joints": ["hip"]},
    {"id": 18, "name": "Side Plank", "category": "core", "target_joints": ["trunk"]},
    {"id": 19, "name": "Bird Dog", "category": "core", "target_joints": ["trunk", "hip", "shoulder"]},
    {"id": 20, "name": "Dead Bug", "category": "core", "target_joints": ["trunk", "hip"]},
    {"id": 21, "name": "Hamstring Curl", "category": "lower", "target_joints": ["knee"]},
    {"id": 22, "name": "Calf Raise", "category": "lower", "target_joints": ["ankle"]},
    {"id": 23, "name": "Terminal Knee Extension", "category": "lower", "target_joints": ["knee"]},
    {"id": 24, "name": "Lateral Walk", "category": "functional", "target_joints": ["hip", "knee"]},
    {"id": 25, "name": "Monster Walk", "category": "functional", "target_joints": ["hip", "knee"]},
    {"id": 26, "name": "Single Leg Bridge", "category": "lower", "target_joints": ["hip", "knee"]},
    {"id": 27, "name": "Plank", "category": "core", "target_joints": ["trunk", "shoulder"]},
    {"id": 28, "name": "Wall Sit", "category": "lower", "target_joints": ["knee"]},
    {"id": 29, "name": "Chop", "category": "core", "target_joints": ["trunk", "shoulder"]},
    {"id": 30, "name": "Band Pull Apart", "category": "upper", "target_joints": ["shoulder"]},
]

# === Pydantic Models ===
class PatientCreate(BaseModel):
    name: str
    age: int
    condition: str  # e.g., "post-TKA", "ACL reconstruction", "rotator cuff repair"
    surgery_date: Optional[str] = None
    therapist_id: str
    target_rom: dict = {}  # joint → target degrees
    milestones: list = []  # functional milestones


class ExercisePlanCreate(BaseModel):
    patient_id: str
    exercises: list[dict]  # [{exercise_id, sets, reps, resistance_kg, frequency_per_day}]
    duration_weeks: int = 8
    notes: str = ""


class SessionCreate(BaseModel):
    patient_id: str
    exercise_plan_id: str
    scheduled_duration_min: int = 30


class TelemetryEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    session_id: Optional[str] = None
    exercise: int = 0
    reps: int = 0
    form_score: int = 100
    form_deviation: int = 0
    joint_angles: dict = {}
    force_mg: int = 0
    weight_g: int = 0
    asymmetry: int = 0
    sensors_connected: int = 0
    band_connected: bool = False
    mat_connected: bool = False


# === Initialize default data ===
def _init_defaults():
    """Initialize default therapist and sample patient."""
    if "therapist_001" not in therapists:
        therapists["therapist_001"] = {
            "id": "therapist_001",
            "name": "Dr. Sarah Chen, DPT",
            "license": "PT-CA-12345",
            "patient_ids": [],
        }
    if "patient_001" not in patients:
        patients["patient_001"] = {
            "id": "patient_001",
            "name": "John Martinez",
            "age": 62,
            "condition": "Post-TKA (Right Knee)",
            "surgery_date": "2026-06-15",
            "therapist_id": "therapist_001",
            "target_rom": {"knee_flexion": 115, "knee_extension": 0},
            "milestones": [
                {"name": "Independent ambulation", "target_days": 7},
                {"name": "90° knee flexion", "target_days": 14},
                {"name": "Full weight-bearing", "target_days": 21},
                {"name": "5× Sit-to-Stand <12s", "target_days": 42},
                {"name": "115° knee flexion", "target_days": 56},
            ],
        }
        therapists["therapist_001"]["patient_ids"].append("patient_001")

    if "patient_001" not in exercise_plans:
        exercise_plans["patient_001"] = {
            "id": "plan_001",
            "patient_id": "patient_001",
            "exercises": [
                {"exercise_id": 12, "name": "Sit to Stand", "sets": 3, "reps": 10, "resistance_kg": 0, "frequency_per_day": 2},
                {"exercise_id": 4, "name": "Knee Extension", "sets": 3, "reps": 15, "resistance_kg": 2, "frequency_per_day": 2},
                {"exercise_id": 16, "name": "Glute Bridge", "sets": 3, "reps": 12, "resistance_kg": 0, "frequency_per_day": 1},
                {"exercise_id": 13, "name": "Single Leg Stance", "sets": 3, "reps": 30, "resistance_kg": 0, "frequency_per_day": 2},
                {"exercise_id": 14, "name": "Heel Raise", "sets": 3, "reps": 15, "resistance_kg": 0, "frequency_per_day": 1},
            ],
            "duration_weeks": 8,
            "start_date": "2026-06-22",
            "notes": "Post-TKA week 1 protocol. Focus on ROM and weight-bearing.",
        }

    if "patient_001" not in recovery_forecasts:
        recovery_forecasts["patient_001"] = {
            "patient_id": "patient_001",
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "current_week": 6,
            "milestones": [
                {"name": "Independent ambulation", "target_days": 7, "predicted_days": 6, "status": "achieved", "confidence": 0.95},
                {"name": "90° knee flexion", "target_days": 14, "predicted_days": 13, "status": "achieved", "confidence": 0.92},
                {"name": "Full weight-bearing", "target_days": 21, "predicted_days": 20, "status": "achieved", "confidence": 0.90},
                {"name": "5× Sit-to-Stand <12s", "target_days": 42, "predicted_days": 38, "status": "on_track", "confidence": 0.85},
                {"name": "115° knee flexion", "target_days": 56, "predicted_days": 52, "status": "on_track", "confidence": 0.82},
            ],
            "overall_progress": 0.65,
            "adherence_rate": 0.78,
            "avg_form_score": 82,
            "risk_flags": [],
        }

    if "patient_001" not in adherence_data:
        adherence_data["patient_001"] = {
            "patient_id": "patient_001",
            "last_7_days": [
                {"date": "2026-07-23", "sessions": 2, "completed": 2, "duration_min": 45},
                {"date": "2026-07-24", "sessions": 1, "completed": 1, "duration_min": 25},
                {"date": "2026-07-25", "sessions": 0, "completed": 0, "duration_min": 0},
                {"date": "2026-07-26", "sessions": 2, "completed": 2, "duration_min": 50},
                {"date": "2026-07-27", "sessions": 1, "completed": 1, "duration_min": 30},
                {"date": "2026-07-28", "sessions": 0, "completed": 0, "duration_min": 0},
                {"date": "2026-07-29", "sessions": 1, "completed": 1, "duration_min": 28},
            ],
            "streak_days": 1,
            "completion_rate_7d": 0.71,
            "dropout_risk_7d": 0.18,
            "avg_sessions_per_day": 1.0,
        }

    if "patient_001" not in form_trends:
        form_trends["patient_001"] = [
            {"date": "2026-07-22", "avg_form_score": 75, "exercises": 5},
            {"date": "2026-07-23", "avg_form_score": 78, "exercises": 5},
            {"date": "2026-07-24", "avg_form_score": 80, "exercises": 5},
            {"date": "2026-07-25", "avg_form_score": 79, "exercises": 4},
            {"date": "2026-07-26", "avg_form_score": 83, "exercises": 5},
            {"date": "2026-07-27", "avg_form_score": 82, "exercises": 5},
            {"date": "2026-07-28", "avg_form_score": 85, "exercises": 5},
            {"date": "2026-07-29", "avg_form_score": 84, "exercises": 5},
        ]

    if "patient_001" not in rom_progress:
        rom_progress["patient_001"] = [
            {"date": "2026-06-22", "knee_flexion": 45, "knee_extension": -10},
            {"date": "2026-06-29", "knee_flexion": 65, "knee_extension": -5},
            {"date": "2026-07-06", "knee_flexion": 80, "knee_extension": 0},
            {"date": "2026-07-13", "knee_flexion": 92, "knee_extension": 0},
            {"date": "2026-07-20", "knee_flexion": 100, "knee_extension": 0},
            {"date": "2026-07-27", "knee_flexion": 105, "knee_extension": 0},
        ]


_init_defaults()

# === API Endpoints ===

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "RehabSync API", "version": "1.0.0"}


@app.get("/api/v1/exercises")
async def list_exercises():
    return {"exercises": EXERCISES, "count": len(EXERCISES)}


@app.get("/api/v1/exercises/{exercise_id}")
async def get_exercise(exercise_id: int):
    for ex in EXERCISES:
        if ex["id"] == exercise_id:
            return ex
    raise HTTPException(status_code=404, detail="Exercise not found")


@app.post("/api/v1/patients")
async def create_patient(patient: PatientCreate):
    pid = f"patient_{len(patients) + 1:03d}"
    patients[pid] = {**patient.dict(), "id": pid}
    if patient.therapist_id in therapists:
        therapists[patient.therapist_id]["patient_ids"].append(pid)
    return {"patient_id": pid, "patient": patients[pid]}


@app.get("/api/v1/patients/{patient_id}")
async def get_patient(patient_id: str):
    if patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")
    return patients[patient_id]


@app.get("/api/v1/therapists/{therapist_id}/patients")
async def get_therapist_patients(therapist_id: str):
    if therapist_id not in therapists:
        raise HTTPException(status_code=404, detail="Therapist not found")
    pids = therapists[therapist_id]["patient_ids"]
    return {"therapist": therapists[therapist_id], "patients": [patients[p] for p in pids if p in patients]}


@app.post("/api/v1/exercise-plans")
async def create_exercise_plan(plan: ExercisePlanCreate):
    if plan.patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")
    plan_id = f"plan_{len(exercise_plans) + 1:03d}"
    exercise_plans[plan.patient_id] = {**plan.dict(), "id": plan_id, "start_date": datetime.now(timezone.utc).date().isoformat()}
    return {"plan_id": plan_id, "plan": exercise_plans[plan.patient_id]}


@app.get("/api/v1/exercise-plans/{patient_id}")
async def get_exercise_plan(patient_id: str):
    if patient_id not in exercise_plans:
        raise HTTPException(status_code=404, detail="No exercise plan found")
    return exercise_plans[patient_id]


@app.post("/api/v1/sessions")
async def create_session(session: SessionCreate):
    sid = f"session_{len(sessions) + 1:06d}"
    sessions[sid] = {
        **session.dict(),
        "id": sid,
        "start_time": datetime.now(timezone.utc).isoformat(),
        "status": "active",
        "reps_total": 0,
        "avg_form_score": 0,
        "exercises_completed": [],
    }
    return {"session_id": sid, "session": sessions[sid]}


@app.get("/api/v1/sessions/{session_id}")
async def get_session(session_id: str):
    if session_id not in sessions:
        raise HTTPException(status_code=404, detail="Session not found")
    return sessions[session_id]


@app.get("/api/v1/sessions")
async def list_sessions(patient_id: str, limit: int = 20):
    result = [s for s in sessions.values() if s.get("patient_id") == patient_id]
    return {"sessions": result[:limit], "count": len(result)}


@app.post("/api/v1/sessions/{session_id}/end")
async def end_session(session_id: str):
    if session_id not in sessions:
        raise HTTPException(status_code=404, detail="Session not found")
    sessions[session_id]["status"] = "completed"
    sessions[session_id]["end_time"] = datetime.now(timezone.utc).isoformat()
    return sessions[session_id]


@app.post("/api/v1/telemetry")
async def receive_telemetry(telem: TelemetryEntry):
    entry = telem.dict()
    entry["received_at"] = datetime.now(timezone.utc).isoformat()
    telemetry_store.append(entry)
    if len(telemetry_store) > 10000:
        telemetry_store = telemetry_store[-5000:]

    # Broadcast to WebSocket clients
    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "telemetry", "data": entry})
        except Exception:
            pass

    return {"status": "ok"}


@app.get("/api/v1/recovery-forecast/{patient_id}")
async def get_recovery_forecast(patient_id: str):
    if patient_id not in recovery_forecasts:
        raise HTTPException(status_code=404, detail="No recovery forecast available")
    return recovery_forecasts[patient_id]


@app.get("/api/v1/adherence/{patient_id}")
async def get_adherence(patient_id: str):
    if patient_id not in adherence_data:
        raise HTTPException(status_code=404, detail="No adherence data available")
    return adherence_data[patient_id]


@app.get("/api/v1/form-trends/{patient_id}")
async def get_form_trends(patient_id: str):
    if patient_id not in form_trends:
        raise HTTPException(status_code=404, detail="No form trend data available")
    return {"trends": form_trends[patient_id]}


@app.get("/api/v1/rom-progress/{patient_id}")
async def get_rom_progress(patient_id: str):
    if patient_id not in rom_progress:
        raise HTTPException(status_code=404, detail="No ROM progress data available")
    return {"progress": rom_progress[patient_id]}


@app.post("/api/v1/alerts")
async def create_alert(patient_id: str, alert_type: str, severity: str, message: str):
    alert = {
        "id": len(alerts) + 1,
        "patient_id": patient_id,
        "alert_type": alert_type,
        "severity": severity,
        "message": message,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "acknowledged": False,
    }
    alerts.append(alert)
    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "alert", "data": alert})
        except Exception:
            pass
    return alert


@app.get("/api/v1/alerts")
async def list_alerts(patient_id: Optional[str] = None, limit: int = 50):
    result = alerts
    if patient_id:
        result = [a for a in alerts if a["patient_id"] == patient_id]
    return {"alerts": result[-limit:], "count": len(result)}


@app.get("/api/v1/reports/{patient_id}")
async def generate_report(patient_id: str):
    """Generate a clinical PDF report for the patient."""
    if patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")

    report = {
        "patient": patients[patient_id],
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "recovery_forecast": recovery_forecasts.get(patient_id, {}),
        "adherence": adherence_data.get(patient_id, {}),
        "form_trends": form_trends.get(patient_id, []),
        "rom_progress": rom_progress.get(patient_id, []),
        "recent_sessions": [s for s in sessions.values() if s.get("patient_id") == patient_id][-10:],
        "recent_alerts": [a for a in alerts if a["patient_id"] == patient_id][-10:],
        "exercise_plan": exercise_plans.get(patient_id, {}),
    }
    # In production: render to PDF using reportlab/weasyprint
    return {"report": report, "format": "json", "note": "PDF generation in production"}


@app.get("/api/v1/ota/check/{node_type}")
async def check_ota(node_type: str):
    """Check for firmware updates for a node type."""
    return {
        "node_type": node_type,
        "latest_version": "1.0.0",
        "available": False,
        "min_version": "1.0.0",
    }


@app.post("/api/v1/ota/firmware")
async def upload_firmware(node_type: str, version: str):
    """Upload a new firmware image for OTA distribution."""
    return {
        "node_type": node_type,
        "version": version,
        "status": "uploaded",
        "url": f"https://ota.rehab-sync.io/fw/{node_type}/{version}",
    }


# === WebSocket for real-time session streaming ===
@app.websocket("/ws/realtime/{patient_id}")
async def websocket_endpoint(ws: WebSocket, patient_id: str):
    await ws.accept()
    websocket_clients.append(ws)
    try:
        while True:
            data = await ws.receive_text()
            msg = json.loads(data)
            # Handle client commands (start/stop session, set exercise, etc.)
            if msg.get("cmd") == "start_session":
                await ws.send_json({"type": "session_started", "patient_id": patient_id})
            elif msg.get("cmd") == "stop_session":
                await ws.send_json({"type": "session_stopped", "patient_id": patient_id})
    except WebSocketDisconnect:
        websocket_clients.remove(ws)


# === MQTT Subscriber (background task) ===
@app.on_event("startup")
async def startup_event():
    """Start MQTT subscriber in background."""
    # In production: asyncio.create_task(mqtt_subscriber())
    pass


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
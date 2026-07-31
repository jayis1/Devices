"""
BloomSync — Cloud Dashboard Backend (FastAPI)

Provides REST API for postpartum maternal health monitoring:
- Patient management (postpartum profiles)
- Vitals tracking (HR, SpO₂, skin temp, HRV, activity)
- Nursing session logging + mastitis risk
- Wound monitoring + infection risk
- Risk assessment (hemorrhage, preeclampsia, wound, mastitis, PPD)
- Recovery trajectory forecasting (6-week)
- PPD screening results
- Obstetrician dashboard
- Clinical PDF reports
- OTA firmware distribution
- Real-time WebSocket streaming

Receives telemetry from Bloom Hub via MQTT.

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
    title="BloomSync API",
    version="1.0.0",
    description="AI-powered postpartum maternal health & recovery monitoring system — cloud backend",
)

# === MQTT Configuration ===
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_TELEMETRY = "bloom-sync/telemetry/#"
MQTT_TOPIC_ALERTS = "bloom-sync/alerts/#"
MQTT_TOPIC_VOICE = "bloom-sync/voice/#"

# === In-memory stores (production: PostgreSQL + InfluxDB) ===
patients: dict[str, dict] = {}
obstetricians: dict[str, dict] = {}
vitals_store: dict[str, list] = {}        # patient_id → list of vitals
nursing_store: dict[str, list] = {}       # patient_id → list of nursing sessions
wound_store: dict[str, list] = {}         # patient_id → list of wound readings
risk_store: dict[str, dict] = {}          # patient_id → latest risk
risk_history: dict[str, list] = {}        # patient_id → list of risk over time
ppd_screens: dict[str, list] = {}         # patient_id → list of PPD screen results
recovery_forecasts: dict[str, dict] = {}
alerts: list[dict] = []
websocket_clients: list[WebSocket] = []

# === Pydantic Models ===
class PatientCreate(BaseModel):
    name: str
    age: int
    delivery_type: str = Field(..., pattern="^(vaginal|cesarean)$")
    delivery_date: str
    gestational_age_weeks: int = 40
    obstetrician_id: str
    parity: int = 1
    complications: list[str] = []
    breastfeeding: bool = True
    wound_type: str = "none"  # "cesarean_incision", "perineal_tear", "none"


class VitalsEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    heart_rate: int = 75
    spo2: int = 98
    skin_temp_c: float = 36.8
    hrv_rmssd_ms: int = 45
    activity_class: int = 0
    battery_pct: int = 100


class NursingEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    temp_left_c: float
    temp_right_c: float
    temp_asym_c: float
    nursing_active: int = 0  # 0=idle, 1=left, 2=right
    position_id: int = 0
    mastitis_risk: int = 0


class WoundEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    wound_temp_c: float
    moisture_pct: int
    ph_value: float
    infection_risk: int = 0


class RiskAssessment(BaseModel):
    patient_id: str
    hemorrhage_risk: int = 0
    preeclampsia_risk: int = 0
    wound_risk: int = 0
    mastitis_risk: int = 0
    ppd_risk: int = 0
    recovery_progress: int = 0
    overall_risk: int = 0
    alert_level: int = 0


class AlertCreate(BaseModel):
    patient_id: str
    alert_type: str
    severity: str = "medium"
    message: str


# === Initialize default data ===
def _init_defaults():
    if "obgyn_001" not in obstetricians:
        obstetricians["obgyn_001"] = {
            "id": "obgyn_001",
            "name": "Dr. Emily Rodriguez, MD",
            "license": "OB-GYN-CA-98765",
            "patient_ids": [],
        }
    if "patient_001" not in patients:
        patients["patient_001"] = {
            "id": "patient_001",
            "name": "Sarah Johnson",
            "age": 32,
            "delivery_type": "cesarean",
            "delivery_date": "2026-07-01",
            "gestational_age_weeks": 39,
            "obstetrician_id": "obgyn_001",
            "parity": 1,
            "complications": ["gestational_diabetes"],
            "breastfeeding": True,
            "wound_type": "cesarean_incision",
            "recovery_day": 30,
        }
        obstetricians["obgyn_001"]["patient_ids"].append("patient_001")

    if "patient_001" not in vitals_store:
        vitals_store["patient_001"] = [
            {"timestamp": 1753900000, "heart_rate": 82, "spo2": 97, "skin_temp_c": 36.9,
             "hrv_rmssd_ms": 38, "activity_class": 4, "battery_pct": 85},
            {"timestamp": 1753936000, "heart_rate": 76, "spo2": 98, "skin_temp_c": 36.8,
             "hrv_rmssd_ms": 45, "activity_class": 0, "battery_pct": 82},
            {"timestamp": 1753972000, "heart_rate": 78, "spo2": 99, "skin_temp_c": 36.7,
             "hrv_rmssd_ms": 42, "activity_class": 1, "battery_pct": 79},
        ]

    if "patient_001" not in nursing_store:
        nursing_store["patient_001"] = [
            {"timestamp": 1753900000, "side": "left", "duration_min": 22, "temp_left_c": 36.8,
             "temp_right_c": 36.7, "temp_asym_c": 0.1, "mastitis_risk": 5},
            {"timestamp": 1753918000, "side": "right", "duration_min": 18, "temp_left_c": 36.7,
             "temp_right_c": 36.9, "temp_asym_c": 0.2, "mastitis_risk": 8},
            {"timestamp": 1753936000, "side": "left", "duration_min": 25, "temp_left_c": 36.9,
             "temp_right_c": 36.7, "temp_asym_c": 0.2, "mastitis_risk": 6},
        ]

    if "patient_001" not in wound_store:
        wound_store["patient_001"] = [
            {"timestamp": 1753900000, "wound_temp_c": 37.1, "moisture_pct": 25,
             "ph_value": 6.7, "infection_risk": 5},
            {"timestamp": 1753936000, "wound_temp_c": 37.0, "moisture_pct": 22,
             "ph_value": 6.6, "infection_risk": 3},
            {"timestamp": 1753972000, "wound_temp_c": 36.9, "moisture_pct": 20,
             "ph_value": 6.5, "infection_risk": 2},
        ]

    if "patient_001" not in risk_store:
        risk_store["patient_001"] = {
            "patient_id": "patient_001",
            "hemorrhage_risk": 5,
            "preeclampsia_risk": 3,
            "wound_risk": 8,
            "mastitis_risk": 6,
            "ppd_risk": 15,
            "recovery_progress": 71,
            "overall_risk": 15,
            "alert_level": 0,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    if "patient_001" not in ppd_screens:
        ppd_screens["patient_001"] = [
            {"date": "2026-07-15", "epds_score": 6, "ppd_screen_positive": False,
             "prosody_score": 0.12, "confidence": 0.88},
            {"date": "2026-07-22", "epds_score": 8, "ppd_screen_positive": False,
             "prosody_score": 0.18, "confidence": 0.85},
            {"date": "2026-07-29", "epds_score": 11, "ppd_screen_positive": False,
             "prosody_score": 0.22, "confidence": 0.86},
        ]

    if "patient_001" not in recovery_forecasts:
        recovery_forecasts["patient_001"] = {
            "patient_id": "patient_001",
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "current_day": 30,
            "total_days": 42,
            "milestones": [
                {"name": "Pain-free ambulation", "target_day": 7, "predicted_day": 6,
                 "status": "achieved", "confidence": 0.95},
                {"name": "Incision healing complete", "target_day": 21, "predicted_day": 20,
                 "status": "achieved", "confidence": 0.90},
                {"name": "Sleep normalization", "target_day": 35, "predicted_day": 33,
                 "status": "on_track", "confidence": 0.82},
                {"name": "Activity baseline restored", "target_day": 42, "predicted_day": 40,
                 "status": "on_track", "confidence": 0.78},
                {"name": "Pain-free nursing", "target_day": 28, "predicted_day": 26,
                 "status": "achieved", "confidence": 0.88},
            ],
            "overall_progress": 0.71,
            "risk_flags": [],
        }


_init_defaults()

# === API Endpoints ===

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "BloomSync API", "version": "1.0.0"}


@app.post("/api/v1/patients")
async def create_patient(patient: PatientCreate):
    pid = f"patient_{len(patients) + 1:03d}"
    patients[pid] = {**patient.dict(), "id": pid, "recovery_day": 1}
    if patient.obstetrician_id in obstetricians:
        obstetricians[patient.obstetrician_id]["patient_ids"].append(pid)
    return {"patient_id": pid, "patient": patients[pid]}


@app.get("/api/v1/patients/{patient_id}")
async def get_patient(patient_id: str):
    if patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")
    return patients[patient_id]


@app.get("/api/v1/obstetricians/{obgyn_id}/patients")
async def get_obgyn_patients(obgyn_id: str):
    if obgyn_id not in obstetricians:
        raise HTTPException(status_code=404, detail="Obstetrician not found")
    pids = obstetricians[obgyn_id]["patient_ids"]
    return {"obstetrician": obstetricians[obgyn_id],
            "patients": [patients[p] for p in pids if p in patients]}


@app.get("/api/v1/obstetricians/{obgyn_id}/overview")
async def get_obgyn_overview(obgyn_id: str):
    if obgyn_id not in obstetricians:
        raise HTTPException(status_code=404, detail="Obstetrician not found")
    pids = obstetricians[obgyn_id]["patient_ids"]
    overview = []
    for pid in pids:
        if pid in patients and pid in risk_store:
            overview.append({
                "patient": patients[pid],
                "risk": risk_store[pid],
                "recovery": recovery_forecasts.get(pid, {}),
            })
    return {"obstetrician": obstetricians[obgyn_id], "patients": overview}


@app.post("/api/v1/telemetry")
async def receive_telemetry(vitals: VitalsEntry):
    entry = vitals.dict()
    entry["received_at"] = datetime.now(timezone.utc).isoformat()
    pid = vitals.patient_id
    if pid not in vitals_store:
        vitals_store[pid] = []
    vitals_store[pid].append(entry)
    if len(vitals_store[pid]) > 100000:
        vitals_store[pid] = vitals_store[pid][-50000:]

    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "vitals", "data": entry})
        except Exception:
            pass
    return {"status": "ok"}


@app.get("/api/v1/vitals/{patient_id}")
async def get_vitals(patient_id: str, limit: int = 100):
    if patient_id not in vitals_store:
        raise HTTPException(status_code=404, detail="No vitals data")
    return {"vitals": vitals_store[patient_id][-limit:], "count": len(vitals_store[patient_id])}


@app.get("/api/v1/vitals/{patient_id}/latest")
async def get_latest_vitals(patient_id: str):
    if patient_id not in vitals_store or not vitals_store[patient_id]:
        raise HTTPException(status_code=404, detail="No vitals data")
    return vitals_store[patient_id][-1]


@app.post("/api/v1/nursing")
async def receive_nursing(nursing: NursingEntry):
    entry = nursing.dict()
    pid = nursing.patient_id
    if pid not in nursing_store:
        nursing_store[pid] = []
    nursing_store[pid].append(entry)
    return {"status": "ok"}


@app.get("/api/v1/nursing/{patient_id}")
async def get_nursing(patient_id: str, limit: int = 50):
    if patient_id not in nursing_store:
        raise HTTPException(status_code=404, detail="No nursing data")
    return {"sessions": nursing_store[patient_id][-limit:]}


@app.get("/api/v1/nursing/{patient_id}/today")
async def get_nursing_today(patient_id: str):
    if patient_id not in nursing_store:
        raise HTTPException(status_code=404, detail="No nursing data")
    today = datetime.now(timezone.utc).date().isoformat()
    today_sessions = [s for s in nursing_store[patient_id] if
                      datetime.fromtimestamp(s["timestamp"], timezone.utc).date().isoformat() == today]
    return {"sessions": today_sessions, "count": len(today_sessions)}


@app.post("/api/v1/wound")
async def receive_wound(wound: WoundEntry):
    entry = wound.dict()
    pid = wound.patient_id
    if pid not in wound_store:
        wound_store[pid] = []
    wound_store[pid].append(entry)
    return {"status": "ok"}


@app.get("/api/v1/wound/{patient_id}")
async def get_wound(patient_id: str, limit: int = 100):
    if patient_id not in wound_store:
        raise HTTPException(status_code=404, detail="No wound data")
    return {"readings": wound_store[patient_id][-limit:]}


@app.get("/api/v1/wound/{patient_id}/risk")
async def get_wound_risk(patient_id: str):
    if patient_id not in wound_store or not wound_store[patient_id]:
        raise HTTPException(status_code=404, detail="No wound data")
    readings = wound_store[patient_id][-20:]
    risk_trend = [{"timestamp": r["timestamp"], "infection_risk": r["infection_risk"],
                   "wound_temp_c": r["wound_temp_c"], "ph_value": r["ph_value"]} for r in readings]
    return {"risk_trend": risk_trend}


@app.get("/api/v1/risk/{patient_id}")
async def get_risk(patient_id: str):
    if patient_id not in risk_store:
        raise HTTPException(status_code=404, detail="No risk assessment available")
    return risk_store[patient_id]


@app.get("/api/v1/risk/{patient_id}/history")
async def get_risk_history(patient_id: str, limit: int = 100):
    if patient_id not in risk_history:
        raise HTTPException(status_code=404, detail="No risk history")
    return {"history": risk_history[patient_id][-limit:]}


@app.post("/api/v1/risk")
async def update_risk(risk: RiskAssessment):
    entry = risk.dict()
    entry["timestamp"] = datetime.now(timezone.utc).isoformat()
    risk_store[risk.patient_id] = entry
    if risk.patient_id not in risk_history:
        risk_history[risk.patient_id] = []
    risk_history[risk.patient_id].append(entry)

    if risk.alert_level >= 3:
        alert = {
            "id": len(alerts) + 1,
            "patient_id": risk.patient_id,
            "alert_type": "critical_risk",
            "severity": "critical",
            "message": f"Critical risk level detected (overall={risk.overall_risk}%)",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "acknowledged": False,
        }
        alerts.append(alert)
        for ws in websocket_clients:
            try:
                await ws.send_json({"type": "alert", "data": alert})
            except Exception:
                pass
    return entry


@app.get("/api/v1/recovery/{patient_id}/forecast")
async def get_recovery_forecast(patient_id: str):
    if patient_id not in recovery_forecasts:
        raise HTTPException(status_code=404, detail="No recovery forecast available")
    return recovery_forecasts[patient_id]


@app.get("/api/v1/recovery/{patient_id}/milestones")
async def get_recovery_milestones(patient_id: str):
    if patient_id not in recovery_forecasts:
        raise HTTPException(status_code=404, detail="No recovery data")
    return {"milestones": recovery_forecasts[patient_id].get("milestones", [])}


@app.get("/api/v1/ppd/{patient_id}/screen")
async def get_ppd_screen(patient_id: str):
    if patient_id not in ppd_screens or not ppd_screens[patient_id]:
        raise HTTPException(status_code=404, detail="No PPD screen data")
    return ppd_screens[patient_id][-1]


@app.get("/api/v1/ppd/{patient_id}/history")
async def get_ppd_history(patient_id: str):
    if patient_id not in ppd_screens:
        raise HTTPException(status_code=404, detail="No PPD screen history")
    return {"screens": ppd_screens[patient_id]}


@app.post("/api/v1/ppd/{patient_id}/voice")
async def submit_voice_prosody(patient_id: str, prosody_score: float, confidence: float = 0.85):
    """Submit voice prosody score for PPD screening."""
    positive = prosody_score > 0.35
    screen = {
        "date": datetime.now(timezone.utc).date().isoformat(),
        "prosody_score": prosody_score,
        "ppd_screen_positive": positive,
        "confidence": confidence,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    if patient_id not in ppd_screens:
        ppd_screens[patient_id] = []
    ppd_screens[patient_id].append(screen)

    if positive:
        alert = {
            "id": len(alerts) + 1,
            "patient_id": patient_id,
            "alert_type": "ppd_screen_positive",
            "severity": "medium",
            "message": "PPD screen positive — follow-up with healthcare provider recommended",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "acknowledged": False,
        }
        alerts.append(alert)
    return screen


@app.post("/api/v1/alerts")
async def create_alert(alert: AlertCreate):
    entry = {
        "id": len(alerts) + 1,
        **alert.dict(),
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "acknowledged": False,
    }
    alerts.append(entry)
    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "alert", "data": entry})
        except Exception:
            pass
    return entry


@app.get("/api/v1/alerts")
async def list_alerts(patient_id: Optional[str] = None, limit: int = 50):
    result = alerts
    if patient_id:
        result = [a for a in alerts if a["patient_id"] == patient_id]
    return {"alerts": result[-limit:], "count": len(result)}


@app.put("/api/v1/alerts/{alert_id}/ack")
async def acknowledge_alert(alert_id: int):
    for a in alerts:
        if a["id"] == alert_id:
            a["acknowledged"] = True
            return a
    raise HTTPException(status_code=404, detail="Alert not found")


@app.get("/api/v1/reports/{patient_id}")
async def generate_report(patient_id: str):
    if patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")
    report = {
        "patient": patients[patient_id],
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "risk_assessment": risk_store.get(patient_id, {}),
        "recovery_forecast": recovery_forecasts.get(patient_id, {}),
        "ppd_screens": ppd_screens.get(patient_id, []),
        "recent_vitals": vitals_store.get(patient_id, [])[-50:],
        "recent_nursing": nursing_store.get(patient_id, [])[-20:],
        "recent_wound": wound_store.get(patient_id, [])[-20:],
        "recent_alerts": [a for a in alerts if a["patient_id"] == patient_id][-10:],
    }
    return {"report": report, "format": "json", "note": "PDF generation in production"}


@app.get("/api/v1/ota/check/{node_type}")
async def check_ota(node_type: str):
    return {"node_type": node_type, "latest_version": "1.0.0", "available": False}


@app.post("/api/v1/ota/firmware")
async def upload_firmware(node_type: str, version: str):
    return {"node_type": node_type, "version": version, "status": "uploaded",
            "url": f"https://ota.bloom-sync.io/fw/{node_type}/{version}"}


# === WebSocket ===
@app.websocket("/ws/realtime/{patient_id}")
async def websocket_endpoint(ws: WebSocket, patient_id: str):
    await ws.accept()
    websocket_clients.append(ws)
    try:
        while True:
            data = await ws.receive_text()
            msg = json.loads(data)
            if msg.get("cmd") == "start_monitoring":
                await ws.send_json({"type": "monitoring_started", "patient_id": patient_id})
            elif msg.get("cmd") == "stop_monitoring":
                await ws.send_json({"type": "monitoring_stopped", "patient_id": patient_id})
    except WebSocketDisconnect:
        websocket_clients.remove(ws)


@app.on_event("startup")
async def startup_event():
    pass  # MQTT subscriber in production


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
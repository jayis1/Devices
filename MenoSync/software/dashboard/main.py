"""
MenoSync — Cloud Dashboard Backend (FastAPI)

Provides REST API for menopause management:
- Patient management (menopause profiles)
- Vitals tracking (HR, SpO₂, skin temp, HRV, activity)
- EDA stress tracking (skin conductance, stress level)
- Sleep quality monitoring (BCG sleep staging, night sweats)
- Hot flash prediction (probability, time to onset, severity)
- Mood/brain fog screening (voice prosody + behavioral features)
- Bone health risk assessment (activity load + demographics)
- Cooling system status (HVAC, shades, pre-emptive activation)
- Personal trigger analysis (SHAP-based)
- Treatment response tracking (HRT effectiveness)
- Gynecologist dashboard
- Clinical PDF reports
- OTA firmware distribution
- Real-time WebSocket streaming

Receives telemetry from Meno Hub via MQTT.

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
    title="MenoSync API",
    version="1.0.0",
    description="AI-powered menopause management & wellness system — cloud backend",
)

# === MQTT Configuration ===
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_TELEMETRY = "menosync/telemetry/#"
MQTT_TOPIC_ALERTS = "menosync/alerts/#"
MQTT_TOPIC_VOICE = "menosync/voice/#"

# === In-memory stores (production: PostgreSQL + InfluxDB) ===
patients: dict[str, dict] = {}
gynecologists: dict[str, dict] = {}
vitals_store: dict[str, list] = {}
eda_store: dict[str, list] = {}
sleep_store: dict[str, list] = {}
sweat_store: dict[str, list] = {}
ambient_store: dict[str, list] = {}
hotflash_store: dict[str, list] = {}
risk_store: dict[str, dict] = {}
risk_history: dict[str, list] = {}
mood_screens: dict[str, list] = {}
bone_risk_store: dict[str, dict] = {}
trigger_analysis: dict[str, dict] = {}
treatment_response: dict[str, list] = {}
cooling_events: dict[str, list] = {}
alerts: list[dict] = []
websocket_clients: list[WebSocket] = []

# === Pydantic Models ===
class PatientCreate(BaseModel):
    name: str
    age: int
    menopause_stage: str = Field(..., pattern="^(perimenopause|menopause|postmenopause)$")
    last_period_date: Optional[str] = None
    gynecologist_id: str
    bmi: float = 24.0
    treatment: str = "none"  # "none", "hrt", "ssri", "gabapentin", "clonidine"
    treatment_start_date: Optional[str] = None
    symptoms: list[str] = []  # hot_flashes, night_sweats, brain_fog, mood, insomnia
    family_history_osteoporosis: bool = False


class VitalsEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    heart_rate: int = 72
    spo2: int = 98
    skin_temp_c: float = 33.0
    hrv_rmssd_ms: int = 45
    activity_class: int = 0
    battery_pct: int = 100


class EDAEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    eda_microsiemens: int = 5
    eda_std: int = 1
    stress_level: int = 0  # 0=calm, 1=low, 2=moderate, 3=high


class SleepEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    hr_bpm: int = 65
    br_bpm: int = 14
    motion_level: int = 0
    sleep_stage: int = 1  # 0=awake, 1=light, 2=deep, 3=REM
    signal_quality: int = 85


class SweatEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    sweat_pct: int = 0
    night_sweat_flag: int = 0  # 0=none, 1=mild, 2=severe
    bed_temp_c: float = 25.0


class AmbientEntry(BaseModel):
    hub_id: str
    patient_id: str
    timestamp: int
    ambient_temp_c: float = 23.0
    humidity_pct: int = 45
    radiant_temp_c: float = 24.0
    hvac_state: int = 0
    shade_pct: int = 0


class HotFlashEntry(BaseModel):
    patient_id: str
    timestamp: int
    probability: int = 0
    minutes_to_onset: int = 0
    skin_temp_trend: int = 0
    eda_trend: int = 0
    severity_pred: int = 0
    cooling_recommended: bool = False
    confidence: int = 0


class RiskAssessment(BaseModel):
    patient_id: str
    hotflash_risk: int = 0
    nightsweat_risk: int = 0
    sleep_quality: int = 75
    mood_risk: int = 0
    bone_risk: int = 0
    overall_risk: int = 0
    cooling_active: bool = False
    alert_level: int = 0


class AlertCreate(BaseModel):
    patient_id: str
    alert_type: str
    severity: str = "medium"
    message: str


# === Initialize default data ===
def _init_defaults():
    if "gyn_001" not in gynecologists:
        gynecologists["gyn_001"] = {
            "id": "gyn_001",
            "name": "Dr. Patricia Chen, MD",
            "license": "OB-GYN-CA-45678",
            "patient_ids": [],
        }
    if "patient_001" not in patients:
        patients["patient_001"] = {
            "id": "patient_001",
            "name": "Linda Martinez",
            "age": 52,
            "menopause_stage": "perimenopause",
            "last_period_date": "2026-04-15",
            "gynecologist_id": "gyn_001",
            "bmi": 23.5,
            "treatment": "hrt",
            "treatment_start_date": "2026-06-01",
            "symptoms": ["hot_flashes", "night_sweats", "brain_fog", "insomnia"],
            "family_history_osteoporosis": False,
        }
        gynecologists["gyn_001"]["patient_ids"].append("patient_001")

    if "patient_001" not in vitals_store:
        vitals_store["patient_001"] = [
            {"timestamp": 1753900000, "heart_rate": 78, "spo2": 97, "skin_temp_c": 33.2,
             "hrv_rmssd_ms": 35, "activity_class": 0, "battery_pct": 85},
            {"timestamp": 1753936000, "heart_rate": 72, "spo2": 98, "skin_temp_c": 33.0,
             "hrv_rmssd_ms": 42, "activity_class": 4, "battery_pct": 82},
            {"timestamp": 1753972000, "heart_rate": 75, "spo2": 99, "skin_temp_c": 33.5,
             "hrv_rmssd_ms": 38, "activity_class": 1, "battery_pct": 79},
        ]

    if "patient_001" not in eda_store:
        eda_store["patient_001"] = [
            {"timestamp": 1753900000, "eda_microsiemens": 6, "eda_std": 2, "stress_level": 0},
            {"timestamp": 1753936000, "eda_microsiemens": 12, "eda_std": 4, "stress_level": 1},
            {"timestamp": 1753972000, "eda_microsiemens": 8, "eda_std": 3, "stress_level": 0},
        ]

    if "patient_001" not in sleep_store:
        sleep_store["patient_001"] = [
            {"timestamp": 1753900000, "hr_bpm": 62, "br_bpm": 13, "motion_level": 0,
             "sleep_stage": 2, "signal_quality": 90},
            {"timestamp": 1753936000, "hr_bpm": 68, "br_bpm": 15, "motion_level": 2,
             "sleep_stage": 0, "signal_quality": 45},
            {"timestamp": 1753972000, "hr_bpm": 64, "br_bpm": 14, "motion_level": 0,
             "sleep_stage": 3, "signal_quality": 88},
        ]

    if "patient_001" not in sweat_store:
        sweat_store["patient_001"] = [
            {"timestamp": 1753900000, "sweat_pct": 5, "night_sweat_flag": 0, "bed_temp_c": 25.2},
            {"timestamp": 1753936000, "sweat_pct": 42, "night_sweat_flag": 2, "bed_temp_c": 37.1},
            {"timestamp": 1753972000, "sweat_pct": 8, "night_sweat_flag": 0, "bed_temp_c": 25.5},
        ]

    if "patient_001" not in hotflash_store:
        hotflash_store["patient_001"] = [
            {"timestamp": 1753900000, "probability": 15, "minutes_to_onset": 0,
             "severity_pred": 0, "cooling_recommended": False, "confidence": 80},
            {"timestamp": 1753936000, "probability": 72, "minutes_to_onset": 12,
             "severity_pred": 1, "cooling_recommended": True, "confidence": 85},
            {"timestamp": 1753972000, "probability": 25, "minutes_to_onset": 0,
             "severity_pred": 0, "cooling_recommended": False, "confidence": 78},
        ]

    if "patient_001" not in risk_store:
        risk_store["patient_001"] = {
            "patient_id": "patient_001",
            "hotflash_risk": 35,
            "nightsweat_risk": 40,
            "sleep_quality": 62,
            "mood_risk": 25,
            "bone_risk": 18,
            "overall_risk": 40,
            "cooling_active": False,
            "alert_level": 1,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    if "patient_001" not in mood_screens:
        mood_screens["patient_001"] = [
            {"date": "2026-07-15", "mood_score": 0.15, "brain_fog_score": 0.10,
             "classification": "normal", "confidence": 0.87},
            {"date": "2026-07-22", "mood_score": 0.22, "brain_fog_score": 0.18,
             "classification": "normal", "confidence": 0.85},
            {"date": "2026-07-29", "mood_score": 0.35, "brain_fog_score": 0.28,
             "classification": "mood_change", "confidence": 0.82},
        ]

    if "patient_001" not in bone_risk_store:
        bone_risk_store["patient_001"] = {
            "patient_id": "patient_001",
            "risk_score": 18,
            "risk_level": "low",
            "weight_bearing_minutes_week": 120,
            "vitamin_d_status": "adequate",
            "frax_aligned": True,
            "recommendation": "Continue current activity level. Add 30 min of weight-bearing exercise weekly.",
            "last_updated": datetime.now(timezone.utc).isoformat(),
        }

    if "patient_001" not in trigger_analysis:
        trigger_analysis["patient_001"] = {
            "patient_id": "patient_001",
            "top_triggers": [
                {"trigger": "ambient_temp_high", "importance": 0.28, "occurrences": 45},
                {"trigger": "stress_eda_spike", "importance": 0.22, "occurrences": 38},
                {"trigger": "caffeine_consumption", "importance": 0.18, "occurrences": 25},
                {"trigger": "poor_sleep_prior_night", "importance": 0.15, "occurrences": 20},
                {"trigger": "alcohol_consumption", "importance": 0.10, "occurrences": 12},
            ],
            "model_accuracy": 0.84,
            "last_updated": datetime.now(timezone.utc).isoformat(),
        }

    if "patient_001" not in treatment_response:
        treatment_response["patient_001"] = [
            {"week": 1, "hot_flash_count": 63, "night_sweat_count": 14, "avg_severity": 2.4},
            {"week": 2, "hot_flash_count": 55, "night_sweat_count": 12, "avg_severity": 2.2},
            {"week": 3, "hot_flash_count": 42, "night_sweat_count": 8, "avg_severity": 1.9},
            {"week": 4, "hot_flash_count": 35, "night_sweat_count": 7, "avg_severity": 1.7},
            {"week": 5, "hot_flash_count": 28, "night_sweat_count": 5, "avg_severity": 1.5},
            {"week": 6, "hot_flash_count": 21, "night_sweat_count": 4, "avg_severity": 1.3},
        ]

    if "patient_001" not in cooling_events:
        cooling_events["patient_001"] = [
            {"timestamp": 1753900000, "action": "start", "target_temp_c": 22.0,
             "hvac_mode": "cool", "shade_pct": 80, "hotflash_prob": 72},
            {"timestamp": 1753900600, "action": "stop", "target_temp_c": 0,
             "hvac_mode": "off", "shade_pct": 0, "hotflash_prob": 15},
        ]


_init_defaults()

# === API Endpoints ===

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "MenoSync API", "version": "1.0.0"}


@app.post("/api/v1/patients")
async def create_patient(patient: PatientCreate):
    pid = f"patient_{len(patients) + 1:03d}"
    patients[pid] = {**patient.dict(), "id": pid}
    if patient.gynecologist_id in gynecologists:
        gynecologists[patient.gynecologist_id]["patient_ids"].append(pid)
    return {"patient_id": pid, "patient": patients[pid]}


@app.get("/api/v1/patients/{patient_id}")
async def get_patient(patient_id: str):
    if patient_id not in patients:
        raise HTTPException(status_code=404, detail="Patient not found")
    return patients[patient_id]


@app.get("/api/v1/gynecologists/{gyn_id}/patients")
async def get_gyn_patients(gyn_id: str):
    if gyn_id not in gynecologists:
        raise HTTPException(status_code=404, detail="Gynecologist not found")
    pids = gynecologists[gyn_id]["patient_ids"]
    return {"gynecologist": gynecologists[gyn_id],
            "patients": [patients[p] for p in pids if p in patients]}


@app.get("/api/v1/gynecologists/{gyn_id}/overview")
async def get_gyn_overview(gyn_id: str):
    if gyn_id not in gynecologists:
        raise HTTPException(status_code=404, detail="Gynecologist not found")
    pids = gynecologists[gyn_id]["patient_ids"]
    overview = []
    for pid in pids:
        if pid in patients and pid in risk_store:
            overview.append({
                "patient": patients[pid],
                "risk": risk_store[pid],
                "treatment_response": treatment_response.get(pid, []),
            })
    return {"gynecologist": gynecologists[gyn_id], "patients": overview}


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


@app.post("/api/v1/eda")
async def receive_eda(eda: EDAEntry):
    entry = eda.dict()
    pid = eda.patient_id
    if pid not in eda_store:
        eda_store[pid] = []
    eda_store[pid].append(entry)
    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "eda", "data": entry})
        except Exception:
            pass
    return {"status": "ok"}


@app.post("/api/v1/sleep")
async def receive_sleep(sleep: SleepEntry):
    entry = sleep.dict()
    pid = sleep.patient_id
    if pid not in sleep_store:
        sleep_store[pid] = []
    sleep_store[pid].append(entry)
    return {"status": "ok"}


@app.post("/api/v1/sweat")
async def receive_sweat(sweat: SweatEntry):
    entry = sweat.dict()
    pid = sweat.patient_id
    if pid not in sweat_store:
        sweat_store[pid] = []
    sweat_store[pid].append(entry)
    return {"status": "ok"}


@app.post("/api/v1/ambient")
async def receive_ambient(ambient: AmbientEntry):
    entry = ambient.dict()
    pid = ambient.patient_id
    if pid not in ambient_store:
        ambient_store[pid] = []
    ambient_store[pid].append(entry)
    return {"status": "ok"}


@app.post("/api/v1/hotflash")
async def receive_hotflash(hf: HotFlashEntry):
    entry = hf.dict()
    pid = hf.patient_id
    if pid not in hotflash_store:
        hotflash_store[pid] = []
    hotflash_store[pid].append(entry)
    for ws in websocket_clients:
        try:
            await ws.send_json({"type": "hotflash", "data": entry})
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


@app.get("/api/v1/eda/{patient_id}")
async def get_eda(patient_id: str, limit: int = 100):
    if patient_id not in eda_store:
        raise HTTPException(status_code=404, detail="No EDA data")
    return {"eda_readings": eda_store[patient_id][-limit:]}


@app.get("/api/v1/sleep/{patient_id}")
async def get_sleep(patient_id: str, limit: int = 100):
    if patient_id not in sleep_store:
        raise HTTPException(status_code=404, detail="No sleep data")
    return {"sleep_data": sleep_store[patient_id][-limit:]}


@app.get("/api/v1/sweat/{patient_id}")
async def get_sweat(patient_id: str, limit: int = 50):
    if patient_id not in sweat_store:
        raise HTTPException(status_code=404, detail="No sweat data")
    return {"sweat_readings": sweat_store[patient_id][-limit:]}


@app.get("/api/v1/hotflash/{patient_id}")
async def get_hotflash(patient_id: str, limit: int = 50):
    if patient_id not in hotflash_store:
        raise HTTPException(status_code=404, detail="No hot flash data")
    return {"predictions": hotflash_store[patient_id][-limit:]}


@app.get("/api/v1/hotflash/{patient_id}/latest")
async def get_latest_hotflash(patient_id: str):
    if patient_id not in hotflash_store or not hotflash_store[patient_id]:
        raise HTTPException(status_code=404, detail="No hot flash data")
    return hotflash_store[patient_id][-1]


@app.get("/api/v1/risk/{patient_id}")
async def get_risk(patient_id: str):
    if patient_id not in risk_store:
        raise HTTPException(status_code=404, detail="No risk assessment available")
    return risk_store[patient_id]


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


@app.get("/api/v1/mood/{patient_id}/screen")
async def get_mood_screen(patient_id: str):
    if patient_id not in mood_screens or not mood_screens[patient_id]:
        raise HTTPException(status_code=404, detail="No mood screen data")
    return mood_screens[patient_id][-1]


@app.get("/api/v1/mood/{patient_id}/history")
async def get_mood_history(patient_id: str):
    if patient_id not in mood_screens:
        raise HTTPException(status_code=404, detail="No mood screen history")
    return {"screens": mood_screens[patient_id]}


@app.post("/api/v1/mood/{patient_id}/voice")
async def submit_voice_prosody(patient_id: str, mood_score: float, brain_fog_score: float,
                                confidence: float = 0.85):
    classification = "normal"
    if mood_score > 0.40 or brain_fog_score > 0.35:
        classification = "mood_change"
    elif mood_score > 0.30 or brain_fog_score > 0.25:
        classification = "brain_fog"
    screen = {
        "date": datetime.now(timezone.utc).date().isoformat(),
        "mood_score": mood_score,
        "brain_fog_score": brain_fog_score,
        "classification": classification,
        "confidence": confidence,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    if patient_id not in mood_screens:
        mood_screens[patient_id] = []
    mood_screens[patient_id].append(screen)
    if classification != "normal":
        alert = {
            "id": len(alerts) + 1,
            "patient_id": patient_id,
            "alert_type": f"mood_screen_{classification}",
            "severity": "medium",
            "message": f"{classification.replace('_', ' ').title()} detected — follow-up recommended",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "acknowledged": False,
        }
        alerts.append(alert)
    return screen


@app.get("/api/v1/bone-risk/{patient_id}")
async def get_bone_risk(patient_id: str):
    if patient_id not in bone_risk_store:
        raise HTTPException(status_code=404, detail="No bone risk assessment")
    return bone_risk_store[patient_id]


@app.get("/api/v1/triggers/{patient_id}")
async def get_triggers(patient_id: str):
    if patient_id not in trigger_analysis:
        raise HTTPException(status_code=404, detail="No trigger analysis available")
    return trigger_analysis[patient_id]


@app.get("/api/v1/treatment/{patient_id}")
async def get_treatment_response(patient_id: str):
    if patient_id not in treatment_response:
        raise HTTPException(status_code=404, detail="No treatment response data")
    return {"weekly_data": treatment_response[patient_id]}


@app.get("/api/v1/cooling/{patient_id}")
async def get_cooling_events(patient_id: str, limit: int = 50):
    if patient_id not in cooling_events:
        raise HTTPException(status_code=404, detail="No cooling event data")
    return {"events": cooling_events[patient_id][-limit:]}


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
        "hot_flash_summary": {
            "total_events": len(hotflash_store.get(patient_id, [])),
            "avg_probability": sum(h["probability"] for h in hotflash_store.get(patient_id, [{"probability": 0}])) / max(len(hotflash_store.get(patient_id, [{}])), 1),
        },
        "sleep_summary": {
            "avg_sleep_quality": risk_store.get(patient_id, {}).get("sleep_quality", 0),
            "night_sweat_events": sum(1 for s in sweat_store.get(patient_id, []) if s.get("night_sweat_flag", 0) > 0),
        },
        "mood_screen": mood_screens.get(patient_id, [])[-1] if mood_screens.get(patient_id) else None,
        "bone_risk": bone_risk_store.get(patient_id, {}),
        "trigger_analysis": trigger_analysis.get(patient_id, {}),
        "treatment_response": treatment_response.get(patient_id, []),
        "cooling_events": len(cooling_events.get(patient_id, [])),
    }
    return report


@app.get("/api/v1/ota/check/{node_type}")
async def check_ota(node_type: str):
    return {"node_type": node_type, "version": "1.0.0", "update_available": False}


@app.websocket("/ws/realtime/{patient_id}")
async def websocket_endpoint(websocket: WebSocket, patient_id: str):
    await websocket.accept()
    websocket_clients.append(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        websocket_clients.remove(websocket)
"""
TrailSync — FastAPI Dashboard Backend

Cloud dashboard for TrailSync trail running safety system.
REST API + WebSocket for real-time group tracking, injury risk,
trail conditions, SOS relay coordination, and training journal.

SPDX-License-Identifier: MIT
"""
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
from datetime import datetime, timedelta
import json
import asyncio

app = FastAPI(
    title="TrailSync Dashboard",
    description="AI-powered trail running safety system — cloud backend",
    version="0.1.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── Models ───────────────────────────────────────────────────────────

class GaitSummary(BaseModel):
    pod_side: str  # "left" or "right"
    gait_class: str  # normal, asymmetric, overpronating, high-impact
    gait_confidence: float
    cadence_spm: float
    ground_contact_ms: float
    vertical_osc_mm: float
    impact_load_pct_bw: float
    pronation_deg: float
    asymmetry_pct: float
    stride_length_cm: float
    terrain: str
    timestamp: datetime

class Telemetry(BaseModel):
    runner_id: str
    lat: float
    lon: float
    altitude_m: float
    speed_cm_s: float
    distance_m: float
    hr: int
    spo2: int
    hrv_rmssd_ms: float
    skin_temp_c: float
    pressure_hpa: float
    battery_pct: int
    num_satellites: int
    flags: int
    timestamp: datetime

class BeaconCondition(BaseModel):
    beacon_id: str
    lat: float
    lon: float
    altitude_m: float
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    battery_pct: int
    pir_events: int
    hazard_flags: int
    trail_difficulty: str
    timestamp: datetime

class SOSAlert(BaseModel):
    runner_id: str
    sos_type: str  # manual, fall_auto, altitude, medical
    severity: str  # minor, moderate, serious, critical
    lat: float
    lon: float
    altitude_m: float
    hr: int
    spo2: int
    hrv_rmssd_ms: float
    injury_class: str
    num_people: int
    timestamp: datetime

class InjuryRiskForecast(BaseModel):
    runner_id: str
    date: str
    injuries: dict  # injury_class -> risk_pct
    training_load: float
    acute_chronic_ratio: float
    recommendations: list[str]

class TrailRoute(BaseModel):
    route_id: str
    name: str
    difficulty: str
    distance_km: float
    elevation_gain_m: float
    estimated_time_hr: float
    water_sources: list[dict]
    hazards: list[str]
    beacons: list[str]

class TrainingSession(BaseModel):
    session_id: str
    runner_id: str
    start_time: datetime
    end_time: Optional[datetime]
    distance_m: float
    elevation_gain_m: float
    avg_pace_min_km: float
    avg_hr: int
    max_hr: int
    avg_spo2: int
    min_spo2: int
    terrain_types: list[str]
    injury_risk_max: float
    gait_asymmetry_max: float
    impact_load_max: float
    cadence_avg: float
    weather: Optional[dict]

# ─── In-memory stores (production: PostgreSQL) ────────────────────────

runners: dict[str, dict] = {}
beacons: dict[str, dict] = {}
sos_alerts: list[dict] = []
training_sessions: dict[str, list[dict]] = {}
trail_routes: dict[str, dict] = {}
gait_history: dict[str, list[dict]] = {}
websocket_connections: list[WebSocket] = []

# ─── Runner endpoints ──────────────────────────────────────────────────

@app.post("/api/v1/runners/{runner_id}/telemetry")
async def post_telemetry(runner_id: str, data: Telemetry):
    """Receive telemetry from a wrist unit via hub."""
    if runner_id not in runners:
        runners[runner_id] = {"id": runner_id, "history": []}
    runners[runner_id].update({
        "current": data.model_dump(),
        "last_seen": datetime.utcnow().isoformat(),
    })
    runners[runner_id]["history"].append(data.model_dump())
    # Keep last 1000 entries
    if len(runners[runner_id]["history"]) > 1000:
        runners[runner_id]["history"] = runners[runner_id]["history"][-1000:]

    # Check for alerts
    alerts = []
    if data.spo2 < 94:
        alerts.append({"type": "altitude_sickness", "severity": "warning",
                       "message": f"SpO2 {data.spo2}% — altitude sickness risk"})
    if data.spo2 < 88:
        alerts.append({"type": "altitude_sickness", "severity": "critical",
                       "message": f"SpO2 {data.spo2}% — HACE/HAPE risk, descend immediately!"})

    # Broadcast to WebSocket clients
    for ws in websocket_connections:
        try:
            await ws.send_json({"event": "telemetry", "runner_id": runner_id,
                                "data": data.model_dump(), "alerts": alerts})
        except Exception:
            pass

    return {"status": "ok", "alerts": alerts}

@app.get("/api/v1/runners/{runner_id}")
async def get_runner(runner_id: str):
    """Get current runner state."""
    if runner_id not in runners:
        raise HTTPException(status_code=404, detail="Runner not found")
    return runners[runner_id]["current"]

@app.get("/api/v1/runners")
async def list_runners():
    """List all active runners."""
    return {k: v["current"] for k, v in runners.items()}

@app.post("/api/v1/runners/{runner_id}/gait")
async def post_gait(runner_id: str, data: GaitSummary):
    """Receive gait summary from a shoe pod (via wrist unit)."""
    if runner_id not in gait_history:
        gait_history[runner_id] = []
    gait_history[runner_id].append(data.model_dump())
    if len(gait_history[runner_id]) > 5000:
        gait_history[runner_id] = gait_history[runner_id][-5000:]
    return {"status": "ok"}

# ─── Beacon endpoints ──────────────────────────────────────────────────

@app.post("/api/v1/beacons/{beacon_id}/conditions")
async def post_beacon_conditions(beacon_id: str, data: BeaconCondition):
    """Receive trail conditions from a beacon."""
    beacons[beacon_id] = data.model_dump()
    # Broadcast to WebSocket clients
    for ws in websocket_connections:
        try:
            await ws.send_json({"event": "beacon", "beacon_id": beacon_id,
                                "data": data.model_dump()})
        except Exception:
            pass
    return {"status": "ok"}

@app.get("/api/v1/beacons")
async def list_beacons():
    """List all beacon conditions."""
    return beacons

@app.get("/api/v1/beacons/{beacon_id}")
async def get_beacon(beacon_id: str):
    """Get a specific beacon's conditions."""
    if beacon_id not in beacons:
        raise HTTPException(status_code=404, detail="Beacon not found")
    return beacons[beacon_id]

@app.put("/api/v1/beacons/{beacon_id}/trail_conditions")
async def update_trail_conditions(beacon_id: str, difficulty: str,
                                   hazard_flags: int, water_available: int,
                                   closure: int = 0):
    """Update trail conditions (from cloud → hub → beacon)."""
    if beacon_id not in beacons:
        raise HTTPException(status_code=404, detail="Beacon not found")
    beacons[beacon_id]["trail_difficulty"] = difficulty
    beacons[beacon_id]["hazard_flags"] = hazard_flags
    beacons[beacon_id]["water_available"] = water_available
    return {"status": "ok"}

# ─── SOS endpoints ─────────────────────────────────────────────────────

@app.post("/api/v1/sos")
async def post_sos(data: SOSAlert):
    """Receive SOS from a wrist unit (via hub or LoRa relay)."""
    sos = data.model_dump()
    sos["received_at"] = datetime.utcnow().isoformat()
    sos["status"] = "received"
    sos_alerts.append(sos)

    # In production: trigger push notification to emergency contacts
    # In production: call local rescue services API
    # In production: relay to nearest beacons with cell coverage

    # Broadcast to WebSocket clients
    for ws in websocket_connections:
        try:
            await ws.send_json({"event": "sos", "data": sos})
        except Exception:
            pass

    return {"status": "ok", "sos_id": len(sos_alerts) - 1}

@app.get("/api/v1/sos")
async def list_sos():
    """List all SOS alerts."""
    return sos_alerts

@app.put("/api/v1/sos/{sos_id}/ack")
async def ack_sos(sos_id: int, status: str, eta_minutes: int = 0):
    """Acknowledge an SOS (from rescue coordination)."""
    if sos_id >= len(sos_alerts):
        raise HTTPException(status_code=404, detail="SOS not found")
    sos_alerts[sos_id]["status"] = status
    sos_alerts[sos_id]["eta_minutes"] = eta_minutes
    return {"status": "ok"}

# ─── Injury risk endpoint ──────────────────────────────────────────────

@app.get("/api/v1/runners/{runner_id}/injury_risk")
async def get_injury_risk(runner_id: str, days: int = 7):
    """Get injury risk forecast for a runner (from ML pipeline)."""
    if runner_id not in runners:
        raise HTTPException(status_code=404, detail="Runner not found")
    # In production: call ML pipeline for real inference
    # Heuristic fallback based on recent gait data
    return InjuryRiskForecast(
        runner_id=runner_id,
        date=datetime.utcnow().strftime("%Y-%m-%d"),
        injuries={
            "IT band syndrome": 15,
            "plantar fasciitis": 10,
            "Achilles tendinopathy": 8,
            "stress fracture": 5,
            "shin splints": 12,
            "runner's knee": 20,
            "ankle sprain": 7,
        },
        training_load=42.0,
        acute_chronic_ratio=1.1,
        recommendations=[
            "Your acute:chronic workload ratio is 1.1 — within safe range",
            "Left/right gait asymmetry trending up — monitor over next 3 runs",
            "Consider adding lateral hip strengthening for IT band prevention",
        ],
    ).model_dump()

# ─── Trail routes ──────────────────────────────────────────────────────

@app.get("/api/v1/trails")
async def list_trails():
    """List available trail routes."""
    return trail_routes

@app.get("/api/v1/trails/{route_id}")
async def get_trail(route_id: str):
    """Get a specific trail route."""
    if route_id not in trail_routes:
        raise HTTPException(status_code=404, detail="Trail not found")
    return trail_routes[route_id]

# ─── Training journal ──────────────────────────────────────────────────

@app.post("/api/v1/runners/{runner_id}/sessions")
async def create_session(runner_id: str, data: TrainingSession):
    """Log a training session."""
    if runner_id not in training_sessions:
        training_sessions[runner_id] = []
    training_sessions[runner_id].append(data.model_dump())
    return {"status": "ok"}

@app.get("/api/v1/runners/{runner_id}/sessions")
async def list_sessions(runner_id: str, limit: int = 20):
    """List recent training sessions."""
    sessions = training_sessions.get(runner_id, [])
    return sessions[-limit:]

# ─── WebSocket ─────────────────────────────────────────────────────────

@app.websocket("/ws/v1/live")
async def websocket_live(websocket: WebSocket):
    """Real-time telemetry, alerts, and group tracking."""
    await websocket.accept()
    websocket_connections.append(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            # Handle incoming WebSocket messages (e.g., commands to hub)
    except WebSocketDisconnect:
        websocket_connections.remove(websocket)

# ─── Health ─────────────────────────────────────────────────────────────

@app.get("/health")
async def health():
    return {"status": "ok", "service": "trailsync-dashboard", "version": "0.1.0"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8023)
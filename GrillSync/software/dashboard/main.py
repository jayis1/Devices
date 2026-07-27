"""
GrillSync — Cloud Dashboard Backend (FastAPI)

Provides REST API for cook sessions, node management, alerts,
telemetry history, meat profiles, ML predictions, reports, and
OTA firmware distribution. Receives telemetry from Grill Hub via MQTT.

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
    title="GrillSync API",
    version="1.0.0",
    description="AI-powered smart grilling & BBQ safety system — cloud backend",
)

# === MQTT Configuration ===
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_TELEMETRY = "grillsync/telemetry/#"
MQTT_TOPIC_ALERTS = "grillsync/alerts/#"

# === In-memory stores (production: PostgreSQL + InfluxDB) ===
cook_sessions: dict[str, dict] = {}
nodes: dict[int, dict] = {}
alerts: list[dict] = []
telemetry_store: list[dict] = []
meat_profiles: list[dict] = []
safety_events: list[dict] = []
thermal_frames: list[dict] = []
websocket_clients: list[WebSocket] = []

# === Initialize default data ===
def _init_defaults():
    """Initialize default meat profiles and node registry."""
    global meat_profiles
    meat_profiles = [
        {
            "id": 0, "name": "Beef", "meat_type": 0,
            "usda_min_temp_c": 62.8,
            "doneness_levels": [
                {"level": 1, "name": "Rare", "temp_c": 52.0},
                {"level": 2, "name": "Medium Rare", "temp_c": 54.0},
                {"level": 3, "name": "Medium", "temp_c": 60.0},
                {"level": 4, "name": "Medium Well", "temp_c": 65.0},
                {"level": 5, "name": "Well Done", "temp_c": 71.0},
            ],
            "rest_time_minutes": 5,
        },
        {
            "id": 1, "name": "Pork", "meat_type": 1,
            "usda_min_temp_c": 62.8,
            "doneness_levels": [
                {"level": 2, "name": "Medium", "temp_c": 65.0},
                {"level": 3, "name": "Medium Well", "temp_c": 70.0},
                {"level": 4, "name": "Well Done", "temp_c": 77.0},
            ],
            "rest_time_minutes": 3,
        },
        {
            "id": 2, "name": "Chicken", "meat_type": 2,
            "usda_min_temp_c": 73.9,
            "doneness_levels": [
                {"level": 5, "name": "Done", "temp_c": 74.0},
            ],
            "rest_time_minutes": 3,
        },
        {
            "id": 3, "name": "Fish", "meat_type": 3,
            "usda_min_temp_c": 62.8,
            "doneness_levels": [
                {"level": 1, "name": "Rare", "temp_c": 45.0},
                {"level": 2, "name": "Medium", "temp_c": 55.0},
                {"level": 3, "name": "Well", "temp_c": 60.0},
            ],
            "rest_time_minutes": 0,
        },
        {
            "id": 4, "name": "Lamb", "meat_type": 4,
            "usda_min_temp_c": 62.8,
            "doneness_levels": [
                {"level": 1, "name": "Rare", "temp_c": 52.0},
                {"level": 2, "name": "Medium Rare", "temp_c": 57.0},
                {"level": 3, "name": "Medium", "temp_c": 63.0},
                {"level": 5, "name": "Well Done", "temp_c": 71.0},
            ],
            "rest_time_minutes": 5,
        },
    ]

    # Register default hub node
    nodes[0] = {
        "id": 0, "type": "hub", "name": "Grill Hub", "online": True
    }


_init_defaults()


# === Pydantic Models ===
class CookSessionStart(BaseModel):
    meat_type: int = Field(ge=0, le=7)
    doneness_target: int = Field(ge=0, le=5)
    probes: list[int] = Field(default=[], max_length=8)
    grill_config: dict = Field(default_factory=dict)


class AlertAck(BaseModel):
    pass


class MeatProfileCreate(BaseModel):
    name: str
    meat_type: int = Field(ge=0, le=7)
    usda_min_temp_c: float
    doneness_levels: list[dict]
    rest_time_minutes: int = 0


class DonenessPredictRequest(BaseModel):
    probe_id: int
    meat_type: int = Field(ge=0, le=7)
    temp_history: list[dict]
    target_temp_c: float


class OTARequest(BaseModel):
    node_ids: list[int]
    version: str


# === Helper: broadcast to WebSocket clients ===
async def broadcast(message: dict):
    """Broadcast a message to all connected WebSocket clients."""
    text = json.dumps(message)
    dead: list[WebSocket] = []
    for ws in websocket_clients:
        try:
            await ws.send_text(text)
        except Exception:
            dead.append(ws)
    for ws in dead:
        websocket_clients.remove(ws)


# === MQTT handler (background task) ===
async def mqtt_listener():
    """
    In production: connect to MQTT broker, subscribe to topics,
    parse incoming telemetry from Grill Hub, store in InfluxDB,
    and broadcast to WebSocket clients.

    Simplified: just keep the task alive.
    """
    # import paho.mqtt.client as mqtt
    # client = mqtt.Client()
    # client.connect(MQTT_BROKER, MQTT_PORT, 60)
    # client.subscribe(MQTT_TOPIC_TELEMETRY)
    # client.on_message = on_mqtt_message
    # client.loop_start()
    while True:
        await asyncio.sleep(1)


@app.on_event("startup")
async def startup():
    asyncio.create_task(mqtt_listener())


# === Cook Session Endpoints ===
@app.post("/api/v1/cook-sessions", status_code=201)
async def start_cook_session(req: CookSessionStart):
    session_id = f"cook_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}"
    session = {
        "id": session_id,
        "start_time": datetime.now(timezone.utc).isoformat(),
        "end_time": None,
        "meat_type": req.meat_type,
        "doneness_target": req.doneness_target,
        "probes": req.probes,
        "grill_config": req.grill_config,
        "status": "active",
    }
    cook_sessions[session_id] = session
    return session


@app.get("/api/v1/cook-sessions/{session_id}")
async def get_cook_session(session_id: str):
    if session_id not in cook_sessions:
        raise HTTPException(404, "Cook session not found")
    return cook_sessions[session_id]


@app.post("/api/v1/cook-sessions/{session_id}/end")
async def end_cook_session(session_id: str):
    if session_id not in cook_sessions:
        raise HTTPException(404, "Cook session not found")
    session = cook_sessions[session_id]
    session["end_time"] = datetime.now(timezone.utc).isoformat()
    session["status"] = "completed"
    return session


# === Node Endpoints ===
@app.get("/api/v1/nodes")
async def list_nodes():
    return {"nodes": list(nodes.values())}


@app.get("/api/v1/nodes/{node_id}/telemetry")
async def get_node_telemetry(node_id: int, limit: int = 100, sensor: Optional[str] = None):
    node_telem = [t for t in telemetry_store if t.get("node_id") == node_id]
    if sensor:
        node_telem = [t for t in node_telem if t.get("sensor_type") == sensor]
    return {"node_id": node_id, "telemetry": node_telem[-limit:]}


# === Alert Endpoints ===
@app.get("/api/v1/alerts")
async def list_alerts(
    severity: Optional[str] = None,
    node_id: Optional[int] = None,
    session_id: Optional[str] = None,
    acknowledged: Optional[bool] = None,
):
    result = alerts
    if severity:
        result = [a for a in result if a.get("severity") == severity]
    if node_id is not None:
        result = [a for a in result if a.get("node_id") == node_id]
    if session_id:
        result = [a for a in result if a.get("session_id") == session_id]
    if acknowledged is not None:
        result = [a for a in result if a.get("acknowledged") == acknowledged]
    return {"alerts": result}


@app.post("/api/v1/alerts/{alert_id}/ack")
async def acknowledge_alert(alert_id: str):
    for a in alerts:
        if a["id"] == alert_id:
            a["acknowledged"] = True
            return {"id": alert_id, "acknowledged": True}
    raise HTTPException(404, "Alert not found")


# === Meat Profile Endpoints ===
@app.get("/api/v1/meat-profiles")
async def list_meat_profiles():
    return {"profiles": meat_profiles}


@app.get("/api/v1/meat-profiles/{profile_id}")
async def get_meat_profile(profile_id: int):
    for p in meat_profiles:
        if p["id"] == profile_id:
            return p
    raise HTTPException(404, "Meat profile not found")


@app.post("/api/v1/meat-profiles", status_code=201)
async def create_meat_profile(req: MeatProfileCreate):
    profile = {
        "id": len(meat_profiles),
        "name": req.name,
        "meat_type": req.meat_type,
        "usda_min_temp_c": req.usda_min_temp_c,
        "doneness_levels": req.doneness_levels,
        "rest_time_minutes": req.rest_time_minutes,
    }
    meat_profiles.append(profile)
    return profile


# === ML Prediction Endpoints ===
@app.post("/api/v1/ml/doneness-predict")
async def predict_doneness(req: DonenessPredictRequest):
    """
    Cloud-side doneness prediction for verification of edge result.
    Uses DonenessNet v2.x (full-resolution, non-quantized).
    """
    # In production: load model, run inference on temp_history
    # Simplified: return based on latest temp
    if req.temp_history:
        latest = req.temp_history[-1]
        current_temp = latest.get("temp_tip_c", 0)
        eta = max(0, int((req.target_temp_c - current_temp) / 0.5))
        doneness = 1  # Simplified
    else:
        eta = 0
        doneness = 0

    return {
        "doneness": doneness,
        "doneness_name": ["Raw","Rare","MR","Medium","MW","Well"][doneness],
        "confidence": 0.94,
        "eta_seconds": eta,
        "model_version": "doneness_v2.1",
    }


@app.post("/api/v1/ml/flareup-predict")
async def predict_flareup(data: dict):
    """Cloud-side flare-up prediction for verification."""
    return {
        "risk": data.get("risk", 0),
        "eta_ms": data.get("eta_ms", 0),
        "model_version": "flareup_v1.3",
    }


# === Thermal Frame Endpoints ===
@app.get("/api/v1/thermal-frames/{session_id}")
async def get_thermal_frames(session_id: str, limit: int = 100):
    frames = [f for f in thermal_frames if f.get("session_id") == session_id]
    return {"session_id": session_id, "frames": frames[-limit:]}


# === Report Endpoints ===
@app.get("/api/v1/reports/cook/{session_id}")
async def generate_cook_report(session_id: str):
    if session_id not in cook_sessions:
        raise HTTPException(404, "Cook session not found")
    # In production: generate PDF with temperature curves, safety log
    session = cook_sessions[session_id]
    return {
        "session_id": session_id,
        "report_type": "cook_summary",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "session": session,
        "safety_events": [e for e in safety_events if e.get("session_id") == session_id],
    }


# === Safety Events ===
@app.get("/api/v1/safety-events")
async def list_safety_events(
    session_id: Optional[str] = None,
    severity: Optional[str] = None,
):
    result = safety_events
    if session_id:
        result = [e for e in result if e.get("session_id") == session_id]
    if severity:
        result = [e for e in result if e.get("severity") == severity]
    return {"events": result}


# === Recipe Endpoints ===
@app.get("/api/v1/recipes")
async def list_recipes():
    return {
        "recipes": [
            {"id": 1, "name": "Reverse Seared Ribeye", "meat_type": 0,
             "target_temp_c": 54.0, "grill_temp_c": 260, "time_min": 45},
            {"id": 2, "name": "BBQ Pulled Pork", "meat_type": 1,
             "target_temp_c": 90.0, "grill_temp_c": 110, "time_min": 480},
            {"id": 3, "name": "Beer Can Chicken", "meat_type": 2,
             "target_temp_c": 74.0, "grill_temp_c": 180, "time_min": 90},
            {"id": 4, "name": "Cedar Plank Salmon", "meat_type": 3,
             "target_temp_c": 55.0, "grill_temp_c": 180, "time_min": 20},
        ]
    }


@app.post("/api/v1/recipes/import")
async def import_recipe(recipe: dict):
    """Import a recipe (XML/JSON format)."""
    return {"status": "imported", "recipe": recipe}


# === Firmware / OTA ===
@app.get("/api/v1/firmware/latest")
async def get_latest_firmware(node_type: str = "hub"):
    versions = {
        "hub": "2.1.0",
        "sentinel": "2.1.0",
        "smoke": "2.0.5",
        "probe": "1.5.2",
    }
    return {
        "version": versions.get(node_type, "1.0.0"),
        "node_type": node_type,
        "checksum": f"sha256:{node_type}2026",
        "size_bytes": 524288,
        "release_notes": "Improved safety detection and ML accuracy",
    }


@app.post("/api/v1/firmware/ota")
async def trigger_ota(req: OTARequest):
    return {
        "status": "ota_started",
        "node_ids": req.node_ids,
        "version": req.version,
    }


# === WebSocket ===
@app.websocket("/ws/realtime")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    websocket_clients.append(ws)
    try:
        while True:
            data = await ws.receive_text()
            # Process client messages (e.g., commands)
            msg = json.loads(data)
            if msg.get("type") == "start_cook":
                await broadcast({"type": "cook_status", "status": "active"})
    except WebSocketDisconnect:
        websocket_clients.remove(ws)


# === Health ===
@app.get("/health")
async def health():
    return {"status": "ok", "service": "grillsync-dashboard", "version": "1.0.0"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
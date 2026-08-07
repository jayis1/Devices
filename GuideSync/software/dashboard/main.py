#!/usr/bin/env python3
"""
GuideSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

Endpoints:
  /api/v1/auth/login            — JWT login
  /api/v1/devices               — List devices
  /api/v1/glasses               — Latest glasses telemetry
  /api/v1/glasses/scene         — Latest scene description
  /api/v1/cane                  — Latest cane telemetry
  /api/v1/band                  — Latest haptic band telemetry
  /api/v1/beacons               — List all nav beacons
  /api/v1/navigation/route      — Get route from A to B
  /api/v1/navigation/destination — Set navigation destination
  /api/v1/navigation/status     — Current nav status
  /api/v1/ocr/request           — Request OCR on image
  /api/v1/alerts                — List alerts
  /api/v1/sos/cancel            — Cancel SOS
  /api/v1/emergency/contacts    — Emergency contacts CRUD
  /api/v1/faces                 — Familiar faces (encrypted)
  /api/v1/location              — Current GPS + indoor position
  /api/v1/ml/scene/history      — Scene detection history
  /api/v1/ml/nav/position       — Latest NavNet position
  /api/v1/ml/fall/history       — Fall event history
  /api/v1/ws                    — Real-time WebSocket
"""
from __future__ import annotations

import asyncio
import json
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel


# ─── Models ────────────────────────────────────────────────────────────────

class Device(BaseModel):
    device_id: str
    device_type: str  # hub, glasses, cane, band, beacon
    name: str
    firmware_version: str
    online: bool
    last_seen: datetime | None = None


class GlassesTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    head_pitch: float
    head_roll: float
    head_yaw: float
    obstacle_class: int
    obstacle_distance_m: float
    obstacle_direction: int
    scene_object_count: int
    primary_object_class: int
    crosswalk_detected: bool
    signal_state: str  # none, walk, don't_walk, countdown
    tof_min_distance_m: float
    tof_hazard_flag: int
    step_count_24h: int
    scenenet_inference_ms: int
    crosswalknet_inference_ms: int


class CaneTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    ultrasonic_distance_m: float
    ultrasonic_valid: bool
    tof_downward_distance_m: float
    dropoff_detected: bool
    stair_detected: bool
    swing_count_24h: int
    cane_tilt_deg: float
    step_count_24h: int


class BandTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    step_count_24h: int
    fall_count_24h: int
    nav_direction: str
    nav_distance_m: int
    sos_armed: bool


class SceneDescription(BaseModel):
    timestamp: datetime
    object_count: int
    objects: list[dict]  # class, distance_m, direction_deg
    crosswalk_state: str
    text_read: str | None


class NavBeacon(BaseModel):
    beacon_id: int
    uuid_short: int
    landmark_name: str
    x: float
    y: float
    floor: int
    battery_v: float


class NavigationRoute(BaseModel):
    destination: str
    steps: list[dict]  # direction, distance_m, landmark
    estimated_minutes: int
    current_step: int


class Alert(BaseModel):
    id: int
    timestamp: datetime
    alert_type: str
    severity: str  # info, warning, critical, emergency
    message: str
    acknowledged: bool = False


class EmergencyContact(BaseModel):
    id: int
    name: str
    phone: str
    relationship: str
    notify_fall: bool = True
    notify_sos: bool = True


class Location(BaseModel):
    gps_lat: float | None
    gps_lon: float | None
    indoor_x: float | None
    indoor_y: float | None
    indoor_floor: int | None
    nearest_beacon: str | None
    nearest_beacon_distance_m: float | None


# ─── Scene object class names (COCO + custom) ──────────────────────────────

OBJECT_CLASSES = {
    0: "person", 1: "bicycle", 2: "car", 3: "motorcycle", 4: "bus",
    5: "truck", 6: "traffic light", 7: "stop sign", 8: "chair", 9: "table",
    10: "bed", 11: "couch", 12: "door", 13: "stairs", 14: "elevator",
    15: "escalator", 16: "bottle", 17: "cup", 18: "laptop", 19: "cell phone",
    20: "book", 21: "clock", 22: "dog", 23: "cat", 24: "white_cane",
    25: "guide_dog", 26: "trash_can", 27: "pole", 28: "wall", 29: "doorway",
    30: "curb", 31: "puddle", 32: "overhanging_branch",
}

SIGNAL_STATES = {0: "none", 1: "walk", 2: "don't_walk", 3: "countdown"}

NAV_DIRECTIONS = {
    0: "straight", 1: "left", 2: "right", 3: "turn_around",
    4: "stop", 5: "arrived", 6: "upstairs", 7: "downstairs",
}


# ─── In-memory stores (production: PostgreSQL + InfluxDB) ──────────────────

class DataStore:
    def __init__(self) -> None:
        self.devices: dict[str, Device] = {}
        self.glasses_telemetry: list[GlassesTelemetry] = []
        self.cane_telemetry: list[CaneTelemetry] = []
        self.band_telemetry: list[BandTelemetry] = []
        self.scene_descriptions: list[SceneDescription] = []
        self.beacons: dict[int, NavBeacon] = {}
        self.alerts: list[dict[str, Any]] = []
        self.emergency_contacts: list[EmergencyContact] = []
        self.location: Location = Location(
            gps_lat=40.7128, gps_lon=-74.0060,
            indoor_x=None, indoor_y=None, indoor_floor=None,
            nearest_beacon=None, nearest_beacon_distance_m=None
        )
        self.navigation: NavigationRoute | None = None
        self.fall_history: list[dict] = []
        self.nav_position: dict | None = None

        # Seed default emergency contacts
        self.emergency_contacts.append(EmergencyContact(
            id=1, name="Jane Doe", phone="+15551234567",
            relationship="spouse"
        ))

        # Seed default beacons
        self.beacons[1] = NavBeacon(
            beacon_id=1, uuid_short=0x0001, landmark_name="Front Door",
            x=0.0, y=0.0, floor=1, battery_v=300
        )
        self.beacons[2] = NavBeacon(
            beacon_id=2, uuid_short=0x0002, landmark_name="Kitchen",
            x=5.0, y=3.0, floor=1, battery_v=295
        )
        self.beacons[3] = NavBeacon(
            beacon_id=3, uuid_short=0x0003, landmark_name="Bathroom",
            x=2.0, y=8.0, floor=1, battery_v=298
        )

    def add_alert(self, alert_type: str, severity: str, message: str) -> None:
        self.alerts.append({
            "id": len(self.alerts),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "alert_type": alert_type,
            "severity": severity,
            "message": message,
            "acknowledged": False,
        })


store = DataStore()

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")


# ─── MQTT Bridge (simulated) ────────────────────────────────────────────────

class MQTTPublisher:
    def __init__(self) -> None:
        self.connected = False

    async def connect(self) -> None:
        self.connected = True
        print("[MQTT] Connected to broker")

    async def publish(self, topic: str, payload: dict) -> None:
        print(f"[MQTT] pub {topic}: {json.dumps(payload)[:120]}")

    async def publish_command(self, device_id: str, command: str, data: dict) -> None:
        topic = f"guidesync/default/cloud/command"
        await self.publish(topic, {"device": device_id, "command": command, **data})


mqtt = MQTTPublisher()


# ─── WebSocket Connection Manager ──────────────────────────────────────────

class ConnectionManager:
    def __init__(self) -> None:
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket) -> None:
        if ws in self.active:
            self.active.remove(ws)

    async def broadcast(self, message: dict) -> None:
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                self.disconnect(ws)


manager = ConnectionManager()


# ─── App ────────────────────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    await mqtt.connect()
    asyncio.create_task(telemetry_simulator())
    yield


app = FastAPI(title="GuideSync", version="1.0.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


async def telemetry_simulator() -> None:
    """Simulate periodic telemetry for demo. Production: MQTT subscriber."""
    while True:
        await asyncio.sleep(30)
        await manager.broadcast({
            "type": "heartbeat",
            "ts": datetime.now(timezone.utc).isoformat()
        })


# ─── Auth ───────────────────────────────────────────────────────────────────

@app.post("/api/v1/auth/login")
async def login(username: str = "demo", password: str = "demo"):
    from jose import jwt as jose_jwt
    import os
    token = jose_jwt.encode(
        {"sub": username, "exp": int(time.time()) + 86400},
        os.environ.get("JWT_SECRET", "guidesync-dev-secret"),
        algorithm="HS256",
    )
    return {"access_token": token, "token_type": "bearer"}


# ─── Devices ────────────────────────────────────────────────────────────────

@app.get("/api/v1/devices")
async def list_devices():
    return list(store.devices.values()) if store.devices else [
        {"device_id": "hub-001", "device_type": "hub", "name": "Vision Hub",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "glasses-001", "device_type": "glasses", "name": "Smart Glasses",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "cane-001", "device_type": "cane", "name": "Smart Cane",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "band-001", "device_type": "band", "name": "Haptic Band",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "beacon-001", "device_type": "beacon", "name": "Front Door Beacon",
         "firmware_version": "1.0.0", "online": True},
    ]


@app.post("/api/v1/devices/{device_id}/ota")
async def trigger_ota(device_id: str, version: str = "1.1.0"):
    await mqtt.publish_command(device_id, "ota", {"version": version})
    return {"status": "ota_triggered", "device": device_id, "version": version}


# ─── Glasses ────────────────────────────────────────────────────────────────

@app.get("/api/v1/glasses", response_model=list[GlassesTelemetry])
async def get_glasses_telemetry():
    return store.glasses_telemetry[-20:] if store.glasses_telemetry else []


@app.get("/api/v1/glasses/scene", response_model=list[SceneDescription])
async def get_scene_descriptions():
    return store.scene_descriptions[-20:] if store.scene_descriptions else []


# ─── Cane ───────────────────────────────────────────────────────────────────

@app.get("/api/v1/cane", response_model=list[CaneTelemetry])
async def get_cane_telemetry():
    return store.cane_telemetry[-20:] if store.cane_telemetry else []


# ─── Band ───────────────────────────────────────────────────────────────────

@app.get("/api/v1/band", response_model=list[BandTelemetry])
async def get_band_telemetry():
    return store.band_telemetry[-20:] if store.band_telemetry else []


# ─── Beacons ────────────────────────────────────────────────────────────────

@app.get("/api/v1/beacons", response_model=list[NavBeacon])
async def list_beacons():
    return list(store.beacons.values())


@app.post("/api/v1/beacons", response_model=NavBeacon)
async def register_beacon(beacon: NavBeacon):
    store.beacons[beacon.beacon_id] = beacon
    return beacon


@app.put("/api/v1/beacons/{beacon_id}", response_model=NavBeacon)
async def update_beacon(beacon_id: int, beacon: NavBeacon):
    if beacon_id not in store.beacons:
        raise HTTPException(404, "Beacon not found")
    store.beacons[beacon_id] = beacon
    return beacon


# ─── Navigation ─────────────────────────────────────────────────────────────

@app.get("/api/v1/navigation/route", response_model=NavigationRoute | None)
async def get_route():
    return store.navigation


@app.post("/api/v1/navigation/destination")
async def set_destination(destination: str):
    # Production: A* route on building graph
    steps = [
        {"direction": "straight", "distance_m": 10, "landmark": "Front Door"},
        {"direction": "left", "distance_m": 5, "landmark": "Hallway"},
        {"direction": "right", "distance_m": 3, "landmark": "Kitchen"},
        {"direction": "arrived", "distance_m": 0, "landmark": destination},
    ]
    store.navigation = NavigationRoute(
        destination=destination, steps=steps,
        estimated_minutes=2, current_step=0
    )
    await manager.broadcast({
        "type": "nav_started", "destination": destination,
        "steps": len(steps)
    })
    return {"status": "navigation_started", "destination": destination}


@app.get("/api/v1/navigation/status")
async def get_nav_status():
    if store.navigation is None:
        return {"active": False}
    return {
        "active": True,
        "destination": store.navigation.destination,
        "current_step": store.navigation.current_step,
        "total_steps": len(store.navigation.steps),
        "eta_minutes": store.navigation.estimated_minutes,
    }


@app.post("/api/v1/navigation/stop")
async def stop_navigation():
    store.navigation = None
    await manager.broadcast({"type": "nav_stopped"})
    return {"status": "navigation_stopped"}


# ─── OCR ────────────────────────────────────────────────────────────────────

@app.post("/api/v1/ocr/request")
async def request_ocr(image_base64: str):
    # Production: decode image, run EAST + CRNN OCR pipeline
    # Return recognized text
    sample_text = "EXIT →"
    store.add_alert("text_read", "info", f"Text read: {sample_text}")
    await manager.broadcast({
        "type": "ocr_result", "text": sample_text
    })
    return {"text": sample_text, "confidence": 0.91}


# ─── Alerts ─────────────────────────────────────────────────────────────────

@app.get("/api/v1/alerts")
async def get_alerts(limit: int = 50):
    return store.alerts[-limit:]


@app.put("/api/v1/alerts/{alert_id}/ack")
async def ack_alert(alert_id: int):
    for a in store.alerts:
        if a["id"] == alert_id:
            a["acknowledged"] = True
            return {"status": "acknowledged", "alert_id": alert_id}
    raise HTTPException(404, "Alert not found")


# ─── Emergency ──────────────────────────────────────────────────────────────

@app.post("/api/v1/sos/cancel")
async def cancel_sos():
    await mqtt.publish_command("hub-001", "sos_cancel", {})
    store.add_alert("sos_cancelled", "info", "SOS cancelled by user")
    await manager.broadcast({"type": "sos_cancelled"})
    return {"status": "sos_cancelled"}


@app.get("/api/v1/emergency/contacts", response_model=list[EmergencyContact])
async def get_contacts():
    return store.emergency_contacts


@app.post("/api/v1/emergency/contacts", response_model=EmergencyContact)
async def add_contact(contact: EmergencyContact):
    contact.id = len(store.emergency_contacts) + 1
    store.emergency_contacts.append(contact)
    return contact


# ─── Faces ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/faces")
async def get_faces():
    # Production: encrypted face embeddings stored on-device
    return {"faces": [], "enabled": False, "privacy_note": "Face recognition is opt-in"}


@app.post("/api/v1/faces")
async def add_face(name: str, embedding_base64: str):
    # Production: encrypt embedding, store on-device + cloud backup
    return {"status": "face_added", "name": name, "encrypted": True}


# ─── Location ───────────────────────────────────────────────────────────────

@app.get("/api/v1/location", response_model=Location)
async def get_location():
    return store.location


# ─── ML Endpoints ───────────────────────────────────────────────────────────

@app.get("/api/v1/ml/scene/history")
async def get_scene_history(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    return [s.model_dump() for s in store.scene_descriptions
            if s.timestamp.timestamp() > cutoff]


@app.get("/api/v1/ml/nav/position")
async def get_nav_position():
    if store.nav_position is None:
        return {"x": 0.0, "y": 0.0, "floor": 1, "confidence": 0.85,
                "nearest_beacon": "Front Door", "nearest_distance_m": 1.2}
    return store.nav_position


@app.get("/api/v1/ml/fall/history")
async def get_fall_history(days: int = 30):
    return store.fall_history[-days:]


# ─── WebSocket ──────────────────────────────────────────────────────────────

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            data = await ws.receive_text()
            msg = json.loads(data)
            if msg.get("type") == "set_destination":
                await set_destination(msg.get("destination", ""))
            elif msg.get("type") == "stop_navigation":
                await stop_navigation()
            elif msg.get("type") == "cancel_sos":
                await cancel_sos()
    except WebSocketDisconnect:
        manager.disconnect(ws)


# ─── Run ────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
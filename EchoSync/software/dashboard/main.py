"""
EchoSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

AI-Powered Sound Awareness & Alert System for the Deaf & Hard-of-Hearing.

Runs the 6-model ML pipeline, stores sound events, serves the mobile app
API, generates accessibility reports, and manages OTA firmware updates.
"""
import asyncio
import json
import logging
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone

import paho.mqtt.client as mqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Depends, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("echosync")

# === Configuration ===
MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC = "echosync/+/telemetry"
INFLUXDB_URL = "http://localhost:8086"
INFLUXDB_TOKEN = "echosync-token"
INFLUXDB_ORG = "echosync"
INFLUXDB_BUCKET = "echosync"

# === Data Models ===

class SoundEvent(BaseModel):
    node_id: int
    sound_class: int
    sound_name: str
    confidence: int
    direction: float
    elevation: int
    duration_ms: int
    temp_c: float
    humidity_pct: float
    db_spl: int
    priority: int  # 0=info, 1=important, 2=emergency
    event_id: int
    timestamp: datetime
    room: str | None = None


class WristBandStatus(BaseModel):
    node_id: int
    battery_v: float
    worn: bool
    sleeping: bool
    last_alert_class: int | None
    last_alert_priority: int | None
    alerts_24h: int
    timestamp: datetime


class DoorTagEvent(BaseModel):
    node_id: int
    battery_v: float
    event_type: int  # 0=knock, 1=doorbell, 2=phone, 3=custom
    confidence: int
    knock_count: int
    event_id: int
    timestamp: datetime


class CustomSoundEnrollment(BaseModel):
    node_id: int
    sound_name: str
    priority: int
    enrollment_samples: int


class HapticConfig(BaseModel):
    intensity: int = 100  # 0-100%
    emergency_pattern: int = 73
    important_pattern: int = 47
    info_pattern: int = 12


# === Sound class names ===
SOUND_NAMES = [
    "SmokeAlarm", "COAlarm", "GlassBreak", "Siren", "Doorbell", "DoorKnock",
    "PhoneRing", "BabyCry", "CarHorn", "DoorOpen", "DoorClose", "Water",
    "DogBark", "AlarmClock", "Microwave", "Dishwasher", "WashingMachine",
    "PersonEnter", "Custom1", "Custom2"
]

PRIORITY_NAMES = {0: "Info", 1: "Important", 2: "Emergency"}

# === In-memory storage (replace with InfluxDB/PostgreSQL in production) ===
sound_events: list[SoundEvent] = []
wrist_band_statuses: dict[int, WristBandStatus] = {}
door_tag_events: list[DoorTagEvent] = []
custom_sounds: list[dict] = []
alert_history: list[dict] = []
ws_clients: list[WebSocket] = []

# === MQTT Client ===
mqtt_client: mqtt.Client | None = None


def on_mqtt_connect(client, userdata, flags, rc):
    logger.info(f"MQTT connected (rc={rc})")
    client.subscribe(MQTT_TOPIC)


def on_mqtt_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload)
        topic = msg.topic
        logger.info(f"MQTT: {topic} → {data}")

        if "type" in data:
            if data["type"] == "sentinel":
                event = SoundEvent(
                    node_id=data["node"],
                    sound_class=data["sound_class"],
                    sound_name=SOUND_NAMES[data["sound_class"]],
                    confidence=data["confidence"],
                    direction=data["direction"],
                    elevation=data.get("elevation", 0),
                    duration_ms=data["duration_ms"],
                    temp_c=data["temp_c"],
                    humidity_pct=data["humidity_pct"],
                    db_spl=data["db_spl"],
                    priority=data["priority"],
                    event_id=data["event_id"],
                    timestamp=datetime.now(timezone.utc),
                )
                sound_events.append(event)
                if len(sound_events) > 10000:
                    sound_events.pop(0)
                # Broadcast to WebSocket clients
                asyncio.run(broadcast_ws(event.model_dump_json()))
                # Check for emergency
                if event.priority == 2:
                    alert_history.append({
                        "type": "emergency",
                        "sound": event.sound_name,
                        "timestamp": event.timestamp.isoformat(),
                        "node": event.node_id,
                    })
            elif data["type"] == "wrist":
                status = WristBandStatus(
                    node_id=data["node"],
                    battery_v=data["battery"],
                    worn=bool(data["worn"]),
                    sleeping=bool(data["sleeping"]),
                    last_alert_class=data.get("last_alert_class"),
                    last_alert_priority=data.get("last_alert_priority"),
                    alerts_24h=data["alerts_24h"],
                    timestamp=datetime.now(timezone.utc),
                )
                wrist_band_statuses[status.node_id] = status
            elif data["type"] == "door":
                event = DoorTagEvent(
                    node_id=data["node"],
                    battery_v=data["battery"],
                    event_type=data["event_type"],
                    confidence=data["confidence"],
                    knock_count=data.get("knock_count", 0),
                    event_id=data["event_id"],
                    timestamp=datetime.now(timezone.utc),
                )
                door_tag_events.append(event)
                if len(door_tag_events) > 5000:
                    door_tag_events.pop(0)
    except Exception as e:
        logger.error(f"MQTT parse error: {e}")


def init_mqtt():
    global mqtt_client
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_message = on_mqtt_message
    try:
        mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        logger.warning(f"MQTT connection failed: {e}")


async def broadcast_ws(message: str):
    for ws in ws_clients[:]:
        try:
            await ws.send_text(message)
        except Exception:
            ws_clients.remove(ws)


# === FastAPI App ===

@asynccontextmanager
async def lifespan(app: FastAPI):
    init_mqtt()
    logger.info("EchoSync backend started")
    yield
    if mqtt_client:
        mqtt_client.loop_stop()


app = FastAPI(
    title="EchoSync — Sound Awareness System",
    description="AI-Powered Sound Awareness & Alert System for the Deaf & Hard-of-Hearing",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# === Auth (simplified) ===
fake_users = {"demo": {"id": 1, "name": "Demo User"}}


def get_current_user(token: str = ""):
    if token not in fake_users:
        raise HTTPException(401, "Unauthorized")
    return fake_users[token]


# === Endpoints ===

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "echosync", "version": "1.0.0"}


@app.get("/api/v1/devices")
async def list_devices():
    devices = []
    for eid, wb in wrist_band_statuses.items():
        devices.append({
            "id": eid, "type": "wrist-band",
            "battery_v": wb.battery_v, "worn": wb.worn, "sleeping": wb.sleeping,
        })
    # Count unique sentinel nodes
    sentinel_nodes = set(e.node_id for e in sound_events[-100:])
    for nid in sentinel_nodes:
        devices.append({"id": nid, "type": "room-sentinel", "online": True})
    return devices


@app.get("/api/v1/sound-events")
async def get_sound_events(limit: int = 50, priority: int | None = None):
    events = sound_events[-limit:] if limit < len(sound_events) else sound_events
    if priority is not None:
        events = [e for e in events if e.priority == priority]
    return [e.model_dump() for e in reversed(events)]


@app.get("/api/v1/sound-events/history")
async def get_sound_history(days: int = 7):
    cutoff = datetime.now(timezone.utc) - timedelta(days=days)
    events = [e for e in sound_events if e.timestamp >= cutoff]
    # Group by day
    daily = {}
    for e in events:
        day = e.timestamp.date().isoformat()
        if day not in daily:
            daily[day] = {"total": 0, "emergency": 0, "important": 0, "info": 0,
                         "classes": {}}
        daily[day]["total"] += 1
        if e.priority == 2:
            daily[day]["emergency"] += 1
        elif e.priority == 1:
            daily[day]["important"] += 1
        else:
            daily[day]["info"] += 1
        cls = e.sound_name
        daily[day]["classes"][cls] = daily[day]["classes"].get(cls, 0) + 1
    return daily


@app.get("/api/v1/room-sentinel")
async def get_sentinel_data():
    # Latest event from each sentinel
    latest = {}
    for e in sound_events:
        if e.node_id not in latest or e.timestamp > latest[e.node_id].timestamp:
            latest[e.node_id] = e
    return {str(k): v.model_dump() for k, v in latest.items()}


@app.get("/api/v1/wrist-band")
async def get_wrist_band_status():
    return {str(k): v.model_dump() for k, v in wrist_band_statuses.items()}


@app.get("/api/v1/door-tag")
async def get_door_tag_events(limit: int = 50):
    events = door_tag_events[-limit:]
    return [e.model_dump() for e in reversed(events)]


@app.get("/api/v1/alerts")
async def get_alerts():
    return alert_history[-100:]


@app.get("/api/v1/sound-awareness-score")
async def get_awareness_score():
    """Sound awareness coverage score (0-100)"""
    total_classes = len(SOUND_NAMES)
    detected_classes = len(set(e.sound_class for e in sound_events[-1000:]))
    coverage = (detected_classes / total_classes) * 100
    return {
        "score": round(coverage, 1),
        "classes_detected": detected_classes,
        "classes_total": total_classes,
        "events_today": len([e for e in sound_events
                            if e.timestamp.date() == datetime.now(timezone.utc).date()]),
    }


@app.get("/api/v1/daily-sound-log")
async def get_daily_log(date: str | None = None):
    if date is None:
        date = datetime.now(timezone.utc).date().isoformat()
    events = [e for e in sound_events if e.timestamp.date().isoformat() == date]
    return {
        "date": date,
        "total_events": len(events),
        "emergency_count": len([e for e in events if e.priority == 2]),
        "important_count": len([e for e in events if e.priority == 1]),
        "info_count": len([e for e in events if e.priority == 0]),
        "most_common": max(set(e.sound_name for e in events),
                          key=lambda x: sum(1 for e in events if e.sound_name == x))
                       if events else None,
        "timeline": [{"time": e.timestamp.isoformat(), "sound": e.sound_name,
                      "priority": e.priority, "direction": e.direction}
                     for e in events],
    }


@app.get("/api/v1/weekly-report")
async def get_weekly_report():
    cutoff = datetime.now(timezone.utc) - timedelta(days=7)
    events = [e for e in sound_events if e.timestamp >= cutoff]
    return {
        "total_events": len(events),
        "emergency_events": len([e for e in events if e.priority == 2]),
        "important_events": len([e for e in events if e.priority == 1]),
        "class_distribution": {SOUND_NAMES[c]: sum(1 for e in events if e.sound_class == c)
                              for c in range(20)},
        "hourly_pattern": {h: sum(1 for e in events if e.timestamp.hour == h)
                          for h in range(24)},
        "most_active_hour": max(range(24),
                               key=lambda h: sum(1 for e in events if e.timestamp.hour == h))
                            if events else None,
    }


@app.get("/api/v1/custom-sounds")
async def get_custom_sounds():
    return custom_sounds


@app.post("/api/v1/custom-sounds/enroll")
async def enroll_custom_sound(enrollment: CustomSoundEnrollment):
    """Start custom sound enrollment on a room sentinel"""
    custom_sounds.append({
        "node_id": enrollment.node_id,
        "name": enrollment.sound_name,
        "priority": enrollment.priority,
        "samples": enrollment.enrollment_samples,
        "status": "enrolling",
        "created_at": datetime.now(timezone.utc).isoformat(),
    })
    # In production: publish MQTT command to sentinel to start recording
    return {"status": "enrollment_started", "node_id": enrollment.node_id}


@app.get("/api/v1/ml/predict/priority")
async def ml_predict_priority(sound_class: int, confidence: int, hour: int):
    """AlertPriority XGBoost prediction"""
    # Simplified heuristic (production: load trained XGBoost model)
    priority_map = {0: 2, 1: 2, 2: 2, 3: 2}  # Emergency classes
    if sound_class in priority_map:
        return {"priority": priority_map[sound_class], "confidence": confidence}
    if 4 <= sound_class <= 8:
        return {"priority": 1, "confidence": confidence}
    return {"priority": 0, "confidence": confidence}


@app.get("/api/v1/ml/predict/class")
async def ml_predict_class():
    """SoundNet classification result (latest)"""
    if sound_events:
        e = sound_events[-1]
        return {
            "sound_class": e.sound_class,
            "sound_name": e.sound_name,
            "confidence": e.confidence,
            "direction": e.direction,
            "priority": e.priority,
        }
    return {"sound_class": -1, "sound_name": "none", "confidence": 0}


@app.get("/api/v1/reports/accessibility")
async def get_accessibility_report():
    """Accessibility-ready PDF report (simplified JSON for now)"""
    cutoff = datetime.now(timezone.utc) - timedelta(days=7)
    events = [e for e in sound_events if e.timestamp >= cutoff]
    return {
        "report_type": "Accessibility Sound Awareness Report",
        "generated": datetime.now(timezone.utc).isoformat(),
        "summary": {
            "total_events_7d": len(events),
            "emergency_events_7d": len([e for e in events if e.priority == 2]),
            "classes_detected": len(set(e.sound_class for e in events)),
            "coverage_score": round(len(set(e.sound_class for e in events)) / 20 * 100, 1),
        },
        "events_by_class": {SOUND_NAMES[c]: sum(1 for e in events if e.sound_class == c)
                           for c in range(20) if any(e.sound_class == c for e in events)},
        "recommendations": [
            "Consider adding room sentinels in uncovered areas" if len(set(e.node_id for e in events)) < 3 else "Good coverage",
            "Emergency sound detection active" if any(e.priority == 2 for e in events) else "No emergencies detected",
        ],
    }


@app.post("/api/v1/haptic/config")
async def set_haptic_config(config: HapticConfig):
    return {"status": "updated", "config": config.model_dump()}


@app.post("/api/v1/display/config")
async def set_display_config(config: dict):
    return {"status": "updated", "config": config}


# === WebSocket ===

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    ws_clients.append(ws)
    try:
        while True:
            data = await ws.receive_text()
            # Echo back for ping/pong
            if data == "ping":
                await ws.send_text("pong")
    except WebSocketDisconnect:
        ws_clients.remove(ws)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
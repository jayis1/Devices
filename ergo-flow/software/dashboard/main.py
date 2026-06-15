"""
ErgoFlow — FastAPI Backend
Real-time posture monitoring dashboard, MQTT bridge, and API server.

Copyright (c) 2026 jayis1. MIT License.
"""

from fastapi import FastAPI, WebSocket, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime, timedelta
import asyncio
import json
import uuid
from contextlib import asynccontextmanager

# ── Models ────────────────────────────────────────────────────────────

class PostureReading(BaseModel):
    timestamp: datetime
    score: int  # 0-100
    posture_class: str  # good, slouch, lean_left, lean_right, hunch
    risk_level: int  # 0=low, 1=medium, 2=high
    duration_seconds: int

class RSIReading(BaseModel):
    timestamp: datetime
    risk_score: int  # 0-100
    risk_level: str  # low, medium, high, critical
    contributing_factors: List[str]

class ActivityReading(BaseModel):
    timestamp: datetime
    activity: str  # typing, mouse, phone, idle, stretch, walk
    confidence: float
    duration_seconds: int

class FocusReading(BaseModel):
    timestamp: datetime
    focus_level: str  # low, medium, high
    score: int  # 0-100

class EnvironmentReading(BaseModel):
    timestamp: datetime
    lux: float
    temperature_c: float
    humidity_pct: float

class DeskStatus(BaseModel):
    timestamp: datetime
    height_mm: int
    motor_state: str  # idle, moving_up, moving_down, error
    current_ma: int

class BreakEvent(BaseModel):
    timestamp: datetime
    break_type: str  # stretch, walk, look_away
    duration_seconds: int
    dismissed: bool
    completed: bool

class NodeHeartbeat(BaseModel):
    timestamp: datetime
    node_id: str
    battery_pct: int
    state: str
    uptime_minutes: int

class DeskCommand(BaseModel):
    cmd: str  # height, preset, stop
    target_mm: Optional[int] = None
    speed_pct: int = 70
    preset: Optional[str] = None  # sit, stand, custom

class LightingCommand(BaseModel):
    r: int
    g: int
    b: int
    w: int
    brightness_pct: int
    mode: str  # manual, circadian, focus, relax

class MonitorTiltCommand(BaseModel):
    tilt_degrees: int  # -15 to +15
    speed_pct: int = 70

class CalibrationCommand(BaseModel):
    target: str  # pressure, imu, desk
    param1: Optional[float] = None
    param2: Optional[float] = None

class WeeklyReport(BaseModel):
    week_start: datetime
    posture_grade: str  # A, B, C, D, F
    avg_posture_score: float
    break_compliance_pct: float
    focus_hours: float
    rsi_risk_trend: List[int]
    recommendations: List[str]

# ── In-memory data store ───────────────────────────────────────────────

class DataStore:
    def __init__(self):
        self.posture_readings: List[PostureReading] = []
        self.rsi_readings: List[RSIReading] = []
        self.activity_readings: List[ActivityReading] = []
        self.focus_readings: List[FocusReading] = []
        self.environment_readings: List[EnvironmentReading] = []
        self.desk_status: List[DeskStatus] = []
        self.break_events: List[BreakEvent] = []
        self.heartbeats: List[NodeHeartbeat] = []
        self.max_readings = 10000
        self.ws_clients: List[WebSocket] = []

    def add_posture(self, reading: PostureReading):
        self.posture_readings.append(reading)
        if len(self.posture_readings) > self.max_readings:
            self.posture_readings = self.posture_readings[-self.max_readings:]

    def add_activity(self, reading: ActivityReading):
        self.activity_readings.append(reading)
        if len(self.activity_readings) > self.max_readings:
            self.activity_readings = self.activity_readings[-self.max_readings:]

    def get_recent_posture(self, minutes: int = 60) -> List[PostureReading]:
        cutoff = datetime.utcnow() - timedelta(minutes=minutes)
        return [r for r in self.posture_readings if r.timestamp >= cutoff]

    def get_current_status(self) -> dict:
        if not self.posture_readings:
            return {"status": "no_data"}
        latest = self.posture_readings[-1]
        return {
            "posture_score": latest.score,
            "posture_class": latest.posture_class,
            "risk_level": latest.risk_level,
            "duration_seconds": latest.duration_seconds,
            "timestamp": latest.timestamp.isoformat(),
        }

store = DataStore()

# ── MQTT Bridge (background task) ─────────────────────────────────────

async def mqtt_bridge():
    """Background task: subscribe to MQTT topics and update data store.

    In production, this connects to the MQTT broker specified in config
    and receives messages from the hub node via the ESP32-C6 bridge.
    """
    # import paho.mqtt.client as mqtt
    # client = mqtt.Client()
    # client.connect("localhost", 1883)
    # client.subscribe("ergoflow/#")
    #
    # def on_message(client, userdata, msg):
    #     topic = msg.topic
    #     payload = json.loads(msg.payload)
    #     if topic == "ergoflow/posture":
    #         store.add_posture(PostureReading(**payload))
    #     elif topic == "ergoflow/activity":
    #         store.add_activity(ActivityReading(**payload))
    #     # ... etc
    #
    # client.on_message = on_message
    # client.loop_start()

    # For now, simulate data for development
    import random
    while True:
        await asyncio.sleep(2)
        # Simulate posture readings
        score = random.randint(40, 100)
        posture_classes = ["good", "slouch", "lean_left", "lean_right", "hunch"]
        weights = [0.5, 0.2, 0.1, 0.1, 0.1]
        posture_class = random.choices(posture_classes, weights=weights)[0]
        if posture_class == "good":
            score = random.randint(70, 100)
        elif posture_class == "hunch":
            score = random.randint(20, 40)

        reading = PostureReading(
            timestamp=datetime.utcnow(),
            score=score,
            posture_class=posture_class,
            risk_level=min(2, (100 - score) // 30),
            duration_seconds=random.randint(0, 3600),
        )
        store.add_posture(reading)

        # Broadcast to WebSocket clients
        for ws in store.ws_clients[:]:
            try:
                await ws.send_json({
                    "type": "posture",
                    "data": reading.dict(),
                })
            except Exception:
                store.ws_clients.remove(ws)


# ── FastAPI app ────────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    task = asyncio.create_task(mqtt_bridge())
    yield
    task.cancel()

app = FastAPI(
    title="ErgoFlow API",
    description="AI-powered adaptive workspace wellness system — REST API + WebSocket",
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


# ── REST Endpoints ─────────────────────────────────────────────────────

@app.get("/api/v1/status")
async def get_status():
    """Get system status — all nodes online, battery levels."""
    return {
        "system": "running",
        "nodes": {
            "hub": {"online": True, "battery_pct": 95},
            "chair_pad": {"online": True, "battery_pct": 72},
            "desk_controller": {"online": True, "battery_pct": 100, "powered": True},
            "wearable_tag_1": {"online": True, "battery_pct": 58},
        },
        "uptime_minutes": 4320,
    }


@app.get("/api/v1/posture/current")
async def get_posture_current():
    """Get current posture score and class."""
    return store.get_current_status()


@app.get("/api/v1/posture/history")
async def get_posture_history(minutes: int = 60):
    """Get posture score time series."""
    readings = store.get_recent_posture(minutes)
    return {
        "readings": [r.dict() for r in readings],
        "count": len(readings),
    }


@app.get("/api/v1/rsi-risk")
async def get_rsi_risk():
    """Get current RSI risk assessment."""
    if not store.posture_readings:
        return {"risk_score": 0, "risk_level": "low"}
    latest = store.posture_readings[-1]
    risk = 100 - latest.score
    return {
        "risk_score": risk,
        "risk_level": "low" if risk < 30 else ("medium" if risk < 60 else "high"),
        "contributing_factors": ["prolonged_sitting"] if latest.duration_seconds > 1800 else [],
    }


@app.get("/api/v1/rsi-risk/trends")
async def get_rsi_risk_trends(days: int = 7):
    """Get RSI risk trends over time."""
    import random
    return {
        "trend": [random.randint(10, 50) for _ in range(days)],
        "labels": [(datetime.utcnow() - timedelta(days=i)).strftime("%a") for i in range(days)],
    }


@app.get("/api/v1/activity/current")
async def get_activity_current():
    """Get current detected activity."""
    if not store.activity_readings:
        return {"activity": "unknown", "confidence": 0}
    latest = store.activity_readings[-1]
    return {"activity": latest.activity, "confidence": latest.confidence}


@app.get("/api/v1/activity/stats")
async def get_activity_stats(period: str = "today"):
    """Get activity breakdown statistics."""
    import random
    return {
        "typing_pct": random.randint(30, 50),
        "mouse_pct": random.randint(15, 25),
        "phone_pct": random.randint(5, 15),
        "idle_pct": random.randint(10, 20),
        "stretch_pct": random.randint(2, 8),
        "walk_pct": random.randint(1, 5),
    }


@app.get("/api/v1/focus/current")
async def get_focus_current():
    """Get current focus level."""
    import random
    levels = ["low", "medium", "high"]
    return {
        "focus_level": random.choice(levels),
        "score": random.randint(30, 90),
    }


@app.get("/api/v1/focus/sessions")
async def get_focus_sessions():
    """Get focus session history."""
    import random
    sessions = []
    for i in range(5):
        sessions.append({
            "start": (datetime.utcnow() - timedelta(hours=i+1)).isoformat(),
            "end": (datetime.utcnow() - timedelta(hours=i, minutes=random.randint(10, 40))).isoformat(),
            "focus_level": random.choice(["low", "medium", "high"]),
            "avg_score": random.randint(40, 85),
        })
    return {"sessions": sessions}


@app.get("/api/v1/breaks")
async def get_breaks():
    """Get break history and compliance."""
    return {
        "breaks_today": 3,
        "breaks_completed": 2,
        "breaks_dismissed": 1,
        "compliance_pct": 66.7,
        "next_break_minutes": 12,
    }


@app.post("/api/v1/breaks/dismiss")
async def dismiss_break():
    """Dismiss current break reminder."""
    return {"status": "dismissed"}


@app.post("/api/v1/desk/height")
async def set_desk_height(cmd: DeskCommand):
    """Set desk height in mm."""
    # In production: send command via MQTT to hub → desk controller
    return {"status": "sent", "target_mm": cmd.target_mm, "speed_pct": cmd.speed_pct}


@app.post("/api/v1/desk/preset")
async def set_desk_preset(cmd: DeskCommand):
    """Set desk to preset position."""
    return {"status": "sent", "preset": cmd.preset}


@app.get("/api/v1/desk/status")
async def get_desk_status():
    """Get current desk status."""
    return {
        "height_mm": 750,
        "motor_state": "idle",
        "current_ma": 0,
        "preset": "sit",
    }


@app.post("/api/v1/lighting")
async def set_lighting(cmd: LightingCommand):
    """Set ambient lighting."""
    # In production: send via MQTT to hub → desk controller
    return {"status": "sent", "r": cmd.r, "g": cmd.g, "b": cmd.b,
            "w": cmd.w, "brightness": cmd.brightness_pct, "mode": cmd.mode}


@app.post("/api/v1/monitor/tilt")
async def set_monitor_tilt(cmd: MonitorTiltCommand):
    """Set monitor tilt angle."""
    if cmd.tilt_degrees < -15 or cmd.tilt_degrees > 15:
        raise HTTPException(400, "Tilt must be between -15 and +15 degrees")
    return {"status": "sent", "tilt_degrees": cmd.tilt_degrees}


@app.get("/api/v1/environment")
async def get_environment():
    """Get current environment readings (lux, temp, humidity)."""
    import random
    return {
        "lux": random.randint(200, 600),
        "temperature_c": round(random.uniform(21, 25), 1),
        "humidity_pct": round(random.uniform(35, 55), 1),
    }


@app.get("/api/v1/analytics/weekly")
async def get_weekly_report():
    """Get weekly health report."""
    import random
    posture_scores = [random.randint(60, 95) for _ in range(7)]
    avg_score = sum(posture_scores) / len(posture_scores)
    grade = "A" if avg_score > 85 else ("B" if avg_score > 70 else ("C" if avg_score > 55 else ("D" if avg_score > 40 else "F")))

    recommendations = []
    if avg_score < 70:
        recommendations.append("Consider taking more frequent breaks — your slouch rate is above average.")
    if avg_score < 80:
        recommendations.append("Try the 'Stand for 5 minutes' break every 30 minutes.")
    recommendations.append("Your posture is best in the morning. Schedule focus work before 11am.")

    return WeeklyReport(
        week_start=datetime.utcnow() - timedelta(days=7),
        posture_grade=grade,
        avg_posture_score=round(avg_score, 1),
        break_compliance_pct=round(random.uniform(50, 90), 1),
        focus_hours=round(random.uniform(3, 6), 1),
        rsi_risk_trend=[random.randint(10, 50) for _ in range(7)],
        recommendations=recommendations,
    ).dict()


@app.post("/api/v1/calibrate")
async def start_calibration(cmd: CalibrationCommand):
    """Start calibration sequence for a node."""
    return {"status": "started", "target": cmd.target}


@app.get("/api/v1/nodes")
async def list_nodes():
    """List all mesh nodes and their status."""
    return {
        "nodes": [
            {"id": "hub", "address": "0x0001", "online": True, "battery_pct": 95,
             "firmware": "1.0.0", "uptime_hours": 72},
            {"id": "chair_pad", "address": "0x0002", "online": True, "battery_pct": 72,
             "firmware": "1.0.0", "uptime_hours": 72},
            {"id": "desk_controller", "address": "0x0003", "online": True, "battery_pct": 100,
             "firmware": "1.0.0", "uptime_hours": 72, "powered": True},
            {"id": "wearable_tag_1", "address": "0x0004", "online": True, "battery_pct": 58,
             "firmware": "1.0.0", "uptime_hours": 48},
        ]
    }


@app.post("/api/v1/ota/start")
async def start_ota(firmware_url: str, target_node: str = "all"):
    """Initiate OTA firmware update."""
    return {"status": "started", "firmware_url": firmware_url, "target": target_node}


# ── WebSocket ───────────────────────────────────────────────────────────

@app.websocket("/ws/v1/realtime")
async def websocket_realtime(websocket: WebSocket):
    """WebSocket for real-time posture/activity stream."""
    await websocket.accept()
    store.ws_clients.append(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            # Handle incoming commands from mobile app
            try:
                cmd = json.loads(data)
                if cmd.get("type") == "desk_height":
                    # Forward to MQTT
                    pass
            except json.JSONDecodeError:
                pass
    except Exception:
        store.ws_clients.remove(websocket)


# ── Health check ───────────────────────────────────────────────────────

@app.get("/health")
async def health():
    return {"status": "ok", "service": "ergoflow-api", "version": "1.0.0"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
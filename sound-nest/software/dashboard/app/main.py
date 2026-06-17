"""
SoundNest Dashboard — FastAPI Backend
AI-powered home acoustic intelligence system.

REST API + WebSocket for real-time sound events, SPL monitoring,
sound dose tracking, and masking control.
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime, timedelta
from enum import IntEnum
import asyncio
import json
import uuid

# ── Sound Event Classes ────────────────────────────────────────────────

class SoundClass(IntEnum):
    SILENCE = 0
    SMOKE_ALARM = 0x01
    CO_ALARM = 0x02
    BURGLAR_ALARM = 0x03
    CAR_ALARM = 0x04
    TIMER_ALARM = 0x05
    DOORBELL = 0x10
    DOOR_KNOCK = 0x11
    DOOR_OPEN = 0x12
    DOOR_CLOSE = 0x13
    SPEECH = 0x20
    CRYING_BABY = 0x21
    COUGH = 0x22
    SNEEZE = 0x23
    LAUGH = 0x24
    SHOUT = 0x25
    DOG_BARK = 0x30
    CAT_MEOW = 0x31
    BIRD_CHIRP = 0x32
    MICROWAVE = 0x40
    BLENDER = 0x41
    DISHWASHER = 0x42
    KETTLE = 0x43
    FAUCET = 0x44
    VACUUM = 0x50
    WASHER = 0x51
    DRYER = 0x52
    FAN = 0x53
    AC_UNIT = 0x54
    TV = 0x55
    MUSIC = 0x56
    CAR_HORN = 0x60
    SIREN = 0x61
    ENGINE = 0x62
    MOTORCYCLE = 0x63
    BICYCLE_BELL = 0x64
    RAIN = 0x70
    THUNDER = 0x71
    WIND = 0x72
    RUNNING_WATER = 0x73
    PHONE_RING = 0x80
    NOTIFICATION = 0x81
    KEYBOARD = 0x82
    GLASS_BREAK = 0x90
    CRASH = 0x91
    GUNSHOT = 0x92
    UNKNOWN = 0xFF


SOUND_CLASS_NAMES = {
    0x00: "Silence", 0x01: "Smoke Alarm", 0x02: "CO Alarm", 0x03: "Burglar Alarm",
    0x04: "Car Alarm", 0x05: "Timer", 0x10: "Doorbell", 0x11: "Door Knock",
    0x12: "Door Open", 0x13: "Door Close", 0x20: "Speech", 0x21: "Crying Baby",
    0x22: "Cough", 0x23: "Sneeze", 0x24: "Laugh", 0x25: "Shout",
    0x30: "Dog Bark", 0x31: "Cat Meow", 0x32: "Bird Chirp",
    0x40: "Microwave", 0x41: "Blender", 0x42: "Dishwasher", 0x43: "Kettle",
    0x44: "Faucet", 0x50: "Vacuum", 0x51: "Washer", 0x52: "Dryer",
    0x53: "Fan", 0x54: "AC Unit", 0x55: "TV", 0x56: "Music",
    0x60: "Car Horn", 0x61: "Siren", 0x62: "Engine", 0x63: "Motorcycle",
    0x64: "Bicycle Bell", 0x70: "Rain", 0x71: "Thunder", 0x72: "Wind",
    0x73: "Running Water", 0x80: "Phone Ring", 0x81: "Notification",
    0x82: "Keyboard", 0x90: "Glass Break", 0x91: "Crash", 0x92: "Gunshot",
    0xFF: "Unknown",
}

ALERT_PRIORITIES = {
    "critical": [SoundClass.SMOKE_ALARM, SoundClass.CO_ALARM,
                 SoundClass.GLASS_BREAK, SoundClass.GUNSHOT],
    "high": [SoundClass.DOORBELL, SoundClass.CRYING_BABY,
             SoundClass.PHONE_RING, SoundClass.SIREN],
    "medium": [SoundClass.DOOR_KNOCK, SoundClass.DOOR_OPEN,
                SoundClass.DOG_BARK, SoundClass.NOTIFICATION],
    "low": [SoundClass.DOOR_CLOSE, SoundClass.FAN, SoundClass.VACUUM],
}

# ── Pydantic Models ────────────────────────────────────────────────────

class SoundEvent(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    hub_id: str
    node_id: str
    sound_class: int
    sound_class_name: str = ""
    confidence: float
    direction_deg: float = 0
    spl_dba: float
    spl_dbc: float = 0
    spl_dbz: float = 0
    peak_spl: float = 0
    duration_ms: int
    room_id: int
    occupancy: bool = False
    temp_c: float = 0
    humidity_pct: float = 0
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class SPLReading(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    hub_id: str
    node_id: str
    spl_dba: float
    spl_dbc: float = 0
    spl_dbz: float = 0
    spl_min: float = 0
    spl_max: float = 0
    spl_eq: float = 0
    spectrum: List[float] = Field(default_factory=lambda: [0.0] * 32)
    occupancy: bool = False
    temp_c: float = 0
    humidity_pct: float = 0
    battery_mv: int = 0
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class DoseRecord(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    hub_id: str
    person_id: str
    node_id: str
    daily_dose_pct: float
    current_spl_dba: float
    twa_dba: float
    peak_dba: float
    exposure_min: float
    activity: int = 0
    battery_mv: int = 0
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class MaskingMode(str, IntEnum):
    OFF = 0
    WHITE_NOISE = 1
    PINK_NOISE = 2
    BROWN_NOISE = 3
    NATURE_RAIN = 4
    NATURE_STREAM = 5
    NATURE_FOREST = 6
    NATURE_OCEAN = 7
    TINNITUS = 8
    PRIVACY = 9
    CUSTOM = 10


class MaskingCommand(BaseModel):
    room_id: int
    mode: int
    volume: int = Field(ge=0, le=100)
    stereo_balance: int = Field(default=50, ge=0, le=100)
    freq_hz: Optional[List[int]] = None
    bandwidth: Optional[int] = None
    fade_in_ms: int = 0
    fade_out_ms: int = 0
    duration_min: int = 0
    adaptive: bool = True


class MaskingStatus(BaseModel):
    room_id: int
    mode: int
    mode_name: str = ""
    volume: int
    stereo_balance: int = 50
    adaptive: bool = True
    active: bool = False


class NodeInfo(BaseModel):
    node_id: str
    node_type: str
    room_id: int
    room_name: str = ""
    online: bool = True
    battery_mv: int = 0
    last_seen: datetime = Field(default_factory=datetime.utcnow)
    uptime_sec: int = 0
    events_today: int = 0
    rssi: int = 0


class AlertRule(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    name: str
    sound_classes: List[int]
    min_confidence: int = 50
    min_spl_dba: float = 0
    priority: str = "medium"
    destinations: List[str] = []  # ["phone", "tag", "display"]
    masking_mode: Optional[int] = None
    enabled: bool = True


class TinnitusProfile(BaseModel):
    person_id: str
    frequency_hz: float = Field(ge=100, le=16000)
    bandwidth: float = Field(ge=0.1, le=2.0)
    volume_pct: int = Field(ge=0, le=100, default=50)
    mask_type: str = "narrowband"  # narrowband, widenoise, notched
    sleep_fade_min: int = 30


# ── Application Setup ──────────────────────────────────────────────────

app = FastAPI(
    title="SoundNest Dashboard",
    description="AI-powered home acoustic intelligence system API",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ── In-Memory Storage (production: use TimescaleDB) ───────────────────

events_db: List[SoundEvent] = []
spl_db: List[SPLReading] = []
dose_db: List[DoseRecord] = []
nodes_db: List[NodeInfo] = []
masking_status: dict = {}
alert_rules: List[AlertRule] = []
tinnitus_profiles: dict = {}

# WebSocket connections for real-time updates
ws_connections: List[WebSocket] = []

# ── Helper Functions ────────────────────────────────────────────────────

def get_sound_class_name(cls: int) -> str:
    return SOUND_CLASS_NAMES.get(cls, "Unknown")

def get_masking_mode_name(mode: int) -> str:
    modes = {
        0: "Off", 1: "White Noise", 2: "Pink Noise", 3: "Brown Noise",
        4: "Rain", 5: "Stream", 6: "Forest", 7: "Ocean",
        8: "Tinnitus", 9: "Privacy", 10: "Custom",
    }
    return modes.get(mode, "Unknown")

def get_alert_priority(cls: int) -> str:
    for priority, classes in ALERT_PRIORITIES.items():
        if cls in classes:
            return priority
    return "info"

async def broadcast_event(event: SoundEvent):
    """Broadcast a sound event to all connected WebSocket clients."""
    data = json.dumps({
        "type": "event",
        "data": event.dict(),
    })
    for ws in ws_connections:
        try:
            await ws.send_text(data)
        except Exception:
            ws_connections.remove(ws)

# ── MQTT Message Handler (called from mqtt_client.py) ──────────────────

async def handle_mqtt_message(topic: str, payload: dict):
    """Process incoming MQTT messages from hub nodes."""
    if "events" in topic:
        event = SoundEvent(
            hub_id=payload.get("hub", "unknown"),
            node_id=payload.get("node", "unknown"),
            sound_class=payload.get("class", 0xFF),
            sound_class_name=get_sound_class_name(payload.get("class", 0xFF)),
            confidence=payload.get("conf", 0),
            direction_deg=payload.get("dir", 0),
            spl_dba=payload.get("dba", 0),
            spl_dbc=payload.get("dbc", 0),
            spl_dbz=payload.get("dbz", 0),
            peak_spl=payload.get("peak", 0),
            duration_ms=payload.get("dur", 0),
            room_id=payload.get("room", 0),
            occupancy=payload.get("occ", False),
            temp_c=payload.get("temp", 0),
            humidity_pct=payload.get("hum", 0),
        )
        events_db.append(event)
        # Keep last 10,000 events
        if len(events_db) > 10000:
            events_db.pop(0)
        await broadcast_event(event)

    elif "spl" in topic:
        reading = SPLReading(
            hub_id=payload.get("hub", "unknown"),
            node_id=payload.get("node", "unknown"),
            spl_dba=payload.get("dba", 0),
            spl_dbc=payload.get("dbc", 0),
            spl_dbz=payload.get("dbz", 0),
            spl_min=payload.get("min", 0),
            spl_max=payload.get("max", 0),
            spl_eq=payload.get("eq", 0),
            temp_c=payload.get("temp", 0),
            humidity_pct=payload.get("hum", 0),
            battery_mv=payload.get("bat", 0),
        )
        spl_db.append(reading)
        if len(spl_db) > 5000:
            spl_db.pop(0)

    elif "dose" in topic:
        dose = DoseRecord(
            hub_id=payload.get("hub", "unknown"),
            person_id=payload.get("person", "unknown"),
            node_id=payload.get("node", "unknown"),
            daily_dose_pct=payload.get("dose_pct", 0),
            current_spl_dba=payload.get("dba", 0),
            twa_dba=payload.get("twa", 0),
            peak_dba=payload.get("peak", 0),
            exposure_min=payload.get("exposure_min", 0),
            activity=payload.get("activity", 0),
            battery_mv=payload.get("bat", 0),
        )
        dose_db.append(dose)
        if len(dose_db) > 5000:
            dose_db.pop(0)

# ── REST API Endpoints ────────────────────────────────────────────────

# --- Sound Events ---

@app.get("/api/v1/events", response_model=List[SoundEvent])
async def list_events(
    limit: int = 100,
    offset: int = 0,
    sound_class: Optional[int] = None,
    room_id: Optional[int] = None,
    min_confidence: int = 0,
    start: Optional[datetime] = None,
    end: Optional[datetime] = None,
):
    """List sound events with optional filtering."""
    result = events_db
    if sound_class is not None:
        result = [e for e in result if e.sound_class == sound_class]
    if room_id is not None:
        result = [e for e in result if e.room_id == room_id]
    if min_confidence > 0:
        result = [e for e in result if e.confidence >= min_confidence]
    if start:
        result = [e for e in result if e.timestamp >= start]
    if end:
        result = [e for e in result if e.timestamp <= end]
    return result[offset:offset + limit]


@app.get("/api/v1/events/{event_id}", response_model=SoundEvent)
async def get_event(event_id: str):
    """Get a specific sound event by ID."""
    for event in events_db:
        if event.id == event_id:
            return event
    raise HTTPException(status_code=404, detail="Event not found")


@app.get("/api/v1/events/stats/summary")
async def events_stats_summary():
    """Get summary statistics for sound events."""
    if not events_db:
        return {"total": 0, "last_24h": 0, "top_classes": [], "avg_spl": 0}
    
    now = datetime.utcnow()
    last_24h = [e for e in events_db if e.timestamp >= now - timedelta(hours=24)]
    
    # Count events by class
    class_counts = {}
    for event in last_24h:
        name = get_sound_class_name(event.sound_class)
        class_counts[name] = class_counts.get(name, 0) + 1
    
    top_classes = sorted(class_counts.items(), key=lambda x: x[1], reverse=True)[:10]
    
    avg_spl = sum(e.spl_dba for e in last_24h) / len(last_24h) if last_24h else 0
    
    return {
        "total": len(events_db),
        "last_24h": len(last_24h),
        "top_classes": [{"name": k, "count": v} for k, v in top_classes],
        "avg_spl": round(avg_spl, 1),
    }


@app.get("/api/v1/events/stats/by-class")
async def events_stats_by_class():
    """Get event counts grouped by sound class."""
    class_counts = {}
    for event in events_db:
        name = get_sound_class_name(event.sound_class)
        class_counts[name] = class_counts.get(name, 0) + 1
    return {"classes": class_counts}


@app.get("/api/v1/events/stats/by-hour")
async def events_stats_by_hour():
    """Get event counts grouped by hour of day."""
    hour_counts = {h: 0 for h in range(24)}
    for event in events_db:
        hour = event.timestamp.hour
        hour_counts[hour] += 1
    return {"hours": hour_counts}


# --- SPL Readings ---

@app.get("/api/v1/spl", response_model=List[SPLReading])
async def list_spl(limit: int = 100, offset: int = 0, room_id: Optional[int] = None):
    """List SPL readings."""
    result = spl_db
    if room_id is not None:
        result = [s for s in result if s.node_id]  # Filter by node in room
    return result[offset:offset + limit]


@app.get("/api/v1/spl/live")
async def spl_live():
    """Get current SPL for each room."""
    rooms = {}
    for reading in spl_db[-20:]:  # Last 20 readings
        rooms[reading.node_id] = {
            "dba": reading.spl_dba,
            "dbc": reading.spl_dbc,
            "dbz": reading.spl_dbz,
            "timestamp": reading.timestamp.isoformat(),
        }
    return {"rooms": rooms}


@app.get("/api/v1/spl/history")
async def spl_history(room_id: Optional[int] = None, hours: int = 24):
    """Get historical SPL data."""
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    result = [s for s in spl_db if s.timestamp >= cutoff]
    if room_id is not None:
        result = [s for s in result if s.node_id]
    return {"readings": len(result), "data": result[-500:]}


@app.get("/api/v1/spl/stats")
async def spl_stats(room_id: Optional[int] = None):
    """Get SPL statistics (Leq, Lmax, L10, L50, L90)."""
    data = spl_db
    if not data:
        return {"leq": 0, "lmax": 0, "lmin": 0, "l10": 0, "l50": 0, "l90": 0}
    
    spls = sorted([s.spl_dba for s in data])
    n = len(spls)
    return {
        "leq": round(sum(s.spl_eq for s in data) / n, 1) if n > 0 else 0,
        "lmax": round(max(s.spl_max for s in data), 1),
        "lmin": round(min(s.spl_min for s in data), 1),
        "l10": round(spls[int(n * 0.1)], 1) if n > 0 else 0,
        "l50": round(spls[int(n * 0.5)], 1) if n > 0 else 0,
        "l90": round(spls[int(n * 0.9)], 1) if n > 0 else 0,
    }


# --- Sound Dose ---

@app.get("/api/v1/dose/today")
async def dose_today(person_id: Optional[str] = None):
    """Get today's sound dose for each person."""
    today = datetime.utcnow().date()
    result = [d for d in dose_db if d.timestamp.date() == today]
    if person_id:
        result = [d for d in result if d.person_id == person_id]
    
    if not result:
        return {"daily_dose_pct": 0, "twa_dba": 0, "peak_dba": 0, "exposure_min": 0}
    
    latest = result[-1]
    return {
        "person_id": latest.person_id,
        "daily_dose_pct": latest.daily_dose_pct,
        "twa_dba": latest.twa_dba,
        "peak_dba": latest.peak_dba,
        "exposure_min": latest.exposure_min,
        "current_spl_dba": latest.current_spl_dba,
        "timestamp": latest.timestamp.isoformat(),
    }


@app.get("/api/v1/dose/history")
async def dose_history(person_id: Optional[str] = None, days: int = 7):
    """Get historical dose data."""
    cutoff = datetime.utcnow() - timedelta(days=days)
    result = [d for d in dose_db if d.timestamp >= cutoff]
    if person_id:
        result = [d for d in result if d.person_id == person_id]
    return {"data": result[-500:]}


@app.get("/api/v1/dose/alerts")
async def dose_alerts():
    """Get current dose alerts (dose > 50%)."""
    alerts = [d for d in dose_db if d.daily_dose_pct > 50]
    return {"alerts": alerts}


@app.post("/api/v1/dose/tinnitus-profile")
async def set_tinnitus_profile(profile: TinnitusProfile):
    """Set tinnitus masking frequency profile."""
    tinnitus_profiles[profile.person_id] = profile
    return {"status": "ok", "profile": profile}


# --- Masking Control ---

@app.post("/api/v1/masking/start")
async def start_masking(cmd: MaskingCommand):
    """Start masking in specified room(s)."""
    masking_status[cmd.room_id] = MaskingStatus(
        room_id=cmd.room_id,
        mode=cmd.mode,
        mode_name=get_masking_mode_name(cmd.mode),
        volume=cmd.volume,
        stereo_balance=cmd.stereo_balance,
        adaptive=cmd.adaptive,
        active=True,
    )
    return {"status": "ok", "command": cmd.dict()}


@app.post("/api/v1/masking/stop")
async def stop_masking(room_id: int):
    """Stop masking in a room."""
    if room_id in masking_status:
        masking_status[room_id].active = False
        masking_status[room_id].mode = 0
        masking_status[room_id].mode_name = "Off"
    return {"status": "ok", "room_id": room_id}


@app.put("/api/v1/masking/settings")
async def update_masking_settings(cmd: MaskingCommand):
    """Update masking settings for a room."""
    masking_status[cmd.room_id] = MaskingStatus(
        room_id=cmd.room_id,
        mode=cmd.mode,
        mode_name=get_masking_mode_name(cmd.mode),
        volume=cmd.volume,
        stereo_balance=cmd.stereo_balance,
        adaptive=cmd.adaptive,
        active=True,
    )
    return {"status": "ok"}


@app.get("/api/v1/masking/status")
async def masking_status_all():
    """Get current masking status for all rooms."""
    return {"rooms": {rid: status.dict() for rid, status in masking_status.items()}}


# --- Node Management ---

@app.get("/api/v1/nodes", response_model=List[NodeInfo])
async def list_nodes():
    """List all registered nodes."""
    return nodes_db


@app.get("/api/v1/nodes/{node_id}", response_model=NodeInfo)
async def get_node(node_id: str):
    """Get node details."""
    for node in nodes_db:
        if node.node_id == node_id:
            return node
    raise HTTPException(status_code=404, detail="Node not found")


@app.put("/api/v1/nodes/{node_id}/config")
async def update_node_config(node_id: str, config: dict):
    """Update node configuration."""
    return {"status": "ok", "node_id": node_id, "config": config}


@app.post("/api/v1/nodes/{node_id}/ota")
async def trigger_ota(node_id: str, firmware_url: str):
    """Trigger OTA firmware update for a node."""
    return {"status": "ok", "node_id": node_id, "firmware_url": firmware_url}


@app.delete("/api/v1/nodes/{node_id}")
async def remove_node(node_id: str):
    """Remove a node from the system."""
    global nodes_db
    nodes_db = [n for n in nodes_db if n.node_id != node_id]
    return {"status": "ok"}


# --- Configuration ---

@app.get("/api/v1/config")
async def get_config():
    """Get system configuration."""
    return {
        "mesh_region": "EU_868",
        "mesh_sf": 7,
        "spl_measurement_interval_s": 5,
        "classification_window_ms": 2000,
        "classification_confidence_threshold": 50,
        "dose_reference_level_dba": 85,
        "dose_exchange_rate_db": 3,
        "masking_auto_start": True,
        "masking_default_volume": 50,
        "alert_rules_count": len(alert_rules),
    }


@app.put("/api/v1/config")
async def update_config(config: dict):
    """Update system configuration."""
    return {"status": "ok", "config": config}


@app.get("/api/v1/config/alert-rules")
async def get_alert_rules():
    """Get all alert rules."""
    return {"rules": alert_rules}


@app.put("/api/v1/config/alert-rules")
async def update_alert_rules(rules: List[AlertRule]):
    """Update alert rules."""
    global alert_rules
    alert_rules = rules
    return {"status": "ok", "count": len(rules)}


@app.get("/api/v1/config/masking-profiles")
async def get_masking_profiles():
    """Get masking profiles."""
    return {
        "profiles": [
            {"id": 1, "name": "White Noise", "mode": 1, "description": "Equal energy per frequency"},
            {"id": 2, "name": "Pink Noise", "mode": 2, "description": "Equal energy per octave"},
            {"id": 3, "name": "Brown Noise", "mode": 3, "description": "Deep, rumbling sound"},
            {"id": 4, "name": "Rain", "mode": 4, "description": "Synthesized rain sounds"},
            {"id": 5, "name": "Stream", "mode": 5, "description": "Flowing water sounds"},
            {"id": 6, "name": "Forest", "mode": 6, "description": "Birds and rustling leaves"},
            {"id": 7, "name": "Ocean", "mode": 7, "description": "Waves and seagulls"},
            {"id": 8, "name": "Tinnitus", "mode": 8, "description": "Personalized tinnitus masking"},
            {"id": 9, "name": "Privacy", "mode": 9, "description": "Directional speech masking"},
        ]
    }


# --- Authentication ---

@app.post("/api/v1/auth/register")
async def register(username: str, password: str, email: str):
    """Register a new user."""
    return {"status": "ok", "username": username}


@app.post("/api/v1/auth/login")
async def login(username: str, password: str):
    """Login and get access token."""
    return {"access_token": "mock_token", "token_type": "bearer"}


@app.post("/api/v1/auth/refresh")
async def refresh_token(token: str):
    """Refresh access token."""
    return {"access_token": "mock_token_new", "token_type": "bearer"}


# ── WebSocket Endpoints ───────────────────────────────────────────────

@app.websocket("/ws/events")
async def ws_events(websocket: WebSocket):
    """Real-time sound event stream."""
    await websocket.accept()
    ws_connections.append(websocket)
    try:
        while True:
            # Keep connection alive, events are pushed from handle_mqtt_message
            data = await websocket.receive_text()
            # Client can send commands via WebSocket too
    except WebSocketDisconnect:
        ws_connections.remove(websocket)


@app.websocket("/ws/spl")
async def ws_spl(websocket: WebSocket):
    """Real-time SPL data stream."""
    await websocket.accept()
    try:
        while True:
            # Send current SPL data every second
            if spl_db:
                latest = spl_db[-1]
                await websocket.send_json({
                    "dba": latest.spl_dba,
                    "dbc": latest.spl_dbc,
                    "dbz": latest.spl_dbz,
                    "timestamp": latest.timestamp.isoformat(),
                })
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        pass


@app.websocket("/ws/dose")
async def ws_dose(websocket: WebSocket):
    """Real-time dose updates."""
    await websocket.accept()
    try:
        while True:
            if dose_db:
                latest = dose_db[-1]
                await websocket.send_json({
                    "dose_pct": latest.daily_dose_pct,
                    "twa_dba": latest.twa_dba,
                    "peak_dba": latest.peak_dba,
                    "timestamp": latest.timestamp.isoformat(),
                })
            await asyncio.sleep(5)
    except WebSocketDisconnect:
        pass


@app.websocket("/ws/alerts")
async def ws_alerts(websocket: WebSocket):
    """Real-time alert stream."""
    await websocket.accept()
    try:
        while True:
            data = await websocket.receive_text()
            # Client can subscribe/unsubscribe to specific alert types
    except WebSocketDisconnect:
        pass


# ── Startup/Shutdown ──────────────────────────────────────────────────

@app.on_event("startup")
async def startup():
    """Initialize MQTT client and seed demo data."""
    # In production, connect to MQTT broker here
    # mqtt_client.connect("soundnest-hub.local", 1883)
    
    # Seed demo data
    nodes_db.extend([
        NodeInfo(node_id="0001", node_type="hub", room_id=0, room_name="Hub",
                 online=True, battery_mv=0, last_seen=datetime.utcnow()),
        NodeInfo(node_id="0002", node_type="room_sensor", room_id=1,
                 room_name="Living Room", online=True, battery_mv=3100,
                 last_seen=datetime.utcnow()),
        NodeInfo(node_id="0003", node_type="room_sensor", room_id=2,
                 room_name="Bedroom", online=True, battery_mv=3050,
                 last_seen=datetime.utcnow()),
        NodeInfo(node_id="0004", node_type="room_sensor", room_id=3,
                 room_name="Office", online=False, battery_mv=2800,
                 last_seen=datetime.utcnow() - timedelta(minutes=5)),
        NodeInfo(node_id="0005", node_type="masking_speaker", room_id=1,
                 room_name="Living Room", online=True, battery_mv=0),
        NodeInfo(node_id="0006", node_type="masking_speaker", room_id=2,
                 room_name="Bedroom", online=True, battery_mv=0),
        NodeInfo(node_id="0007", node_type="wearable_tag", room_id=0,
                 room_name="Person 1", online=True, battery_mv=3900),
    ])


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
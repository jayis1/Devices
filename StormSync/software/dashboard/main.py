"""
StormSync Cloud Dashboard — FastAPI Backend

Endpoints:
  /api/v1/auth/login          — JWT login
  /api/v1/devices             — Device list
  /api/v1/sump                — Sump pit readings
  /api/v1/sump/history        — Historical sump data
  /api/v1/soil                — Soil saturation readings
  /api/v1/weather             — Weather data
  /api/v1/actuator/status     — Flood actuator status
  /api/v1/actuator/valve      — Control backflow valve
  /api/v1/actuator/pump       — Control backup pump
  /api/v1/alerts              — Alerts
  /api/v1/flood-score         — StormSync Score (0-100)
  /api/v1/pump-health         — Sump pump health report
  /api/v1/flood-forecast      — 6-hour water level forecast
  /api/v1/soil-forecast       — 24-hour soil saturation forecast
  /api/v1/water-usage         — Pump activity + water volume stats
  /api/v1/ml/predict/flood    — Flood risk prediction
  /api/v1/ml/predict/pump     — Pump failure prediction
  /api/v1/ws                  — WebSocket real-time updates

MQTT Topics (subscribed):
  stormsync/{user}/hub/telemetry  — Telemetry from nodes (via hub)
  stormsync/{user}/hub/sump       — Sump pit detailed readings
  stormsync/{user}/hub/status     — Hub heartbeat
"""

import asyncio
import json
import time
from datetime import datetime, timedelta, timezone
from typing import Optional

import numpy as np
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
INFLUXDB_URL = "http://localhost:8086"
INFLUXDB_TOKEN = "stormsync-token"
INFLUXDB_ORG = "stormsync"
INFLUXDB_BUCKET = "stormsync_telemetry"
JWT_SECRET = "stormsync-secret-change-in-production"
JWT_ALGORITHM = "HS256"
JWT_EXPIRE_HOURS = 24

# ---------------------------------------------------------------------------
# App Setup
# ---------------------------------------------------------------------------

app = FastAPI(
    title="StormSync Dashboard API",
    version="1.0.0",
    description="AI-powered home flood prediction & sump pump intelligence system",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# Data Models
# ---------------------------------------------------------------------------

class LoginRequest(BaseModel):
    email: str
    password: str

class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int = JWT_EXPIRE_HOURS * 3600

class DeviceInfo(BaseModel):
    node_id: int
    node_type: str
    name: str
    battery_v: float
    last_seen: Optional[datetime]
    online: bool
    firmware_version: str

class SumpReading(BaseModel):
    node_id: int
    timestamp: datetime
    water_level_mm: int
    water_level_pct: float
    pump_current_ma: int
    pump_status: str
    flow_rate_lpm: float
    water_temp_c: float
    vibration_rms_mg: int
    vibration_peak_mg: int
    mains_power: bool
    pump_runtime_today_min: int
    battery_v: float

class SoilReading(BaseModel):
    node_id: int
    timestamp: datetime
    moisture_15_pct: float
    moisture_45_pct: float
    moisture_90_pct: float
    pore_pressure_kpa: float
    temp_15_c: int
    temp_45_c: int
    temp_90_c: int
    battery_mv: int

class WeatherData(BaseModel):
    timestamp: datetime
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    pressure_trend: str
    wind_speed_ms: float
    wind_dir_deg: int
    rain_mm: float
    forecast: Optional[dict] = None

class ActuatorStatus(BaseModel):
    valve_status: str  # "open", "closed", "moving"
    pump_relay: bool
    float_switch: bool
    alarm_status: bool
    mains_power: bool
    battery_v: float
    battery_health_pct: int

class ValveControl(BaseModel):
    action: str  # "open" or "close"

class PumpControl(BaseModel):
    action: str  # "on" or "off"

class AlertItem(BaseModel):
    id: str
    node_id: int
    alert_type: str
    severity: int
    message: str
    timestamp: datetime
    acknowledged: bool

class FloodScore(BaseModel):
    score: int
    risk_level: str  # "low", "moderate", "high", "critical"
    confidence: float
    factors: dict
    recommendations: list[str]

class PumpHealth(BaseModel):
    classification: str
    confidence: float
    predicted_time_to_failure_days: Optional[int]
    vibration_trend: str
    current_draw_trend: str
    cycle_count_today: int
    avg_cycle_duration_s: int
    maintenance_recommended: bool
    last_maintenance: Optional[str]
    notes: str

class FloodForecast(BaseModel):
    forecast_horizon_hours: int
    resolution_minutes: int
    predictions: list[dict]
    max_predicted_level_mm: int
    max_predicted_time: str
    flood_threshold_mm: int
    flood_predicted: bool

class WaterUsage(BaseModel):
    pump_cycles_today: int
    total_pump_runtime_min: int
    estimated_volume_liters: int
    daily_usage: list[dict]

# ---------------------------------------------------------------------------
# Mock Data
# ---------------------------------------------------------------------------

MOCK_DEVICES = [
    {"node_id": 0, "node_type": "hub", "name": "Hub", "battery_v": 0,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 1, "node_type": "sump", "name": "Sump Pit Sentinel", "battery_v": 13.2,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 2, "node_type": "soil", "name": "Soil Probe - North", "battery_mv": 330,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 3, "node_type": "soil", "name": "Soil Probe - South", "battery_mv": 315,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 4, "node_type": "soil", "name": "Soil Probe - East", "battery_mv": 325,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 5, "node_type": "soil", "name": "Soil Probe - West", "battery_mv": 320,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 6, "node_type": "weather", "name": "Weather Sentinel", "battery_mv": 330,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 7, "node_type": "actuator", "name": "Flood Actuator", "battery_v": 13.1,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
]

MOCK_ALERTS = [
    AlertItem(id="a1", node_id=1, alert_type="pump_degradation",
              severity=2, message="Sump pump bearing wear detected (class 1, 87% confidence)",
              timestamp=datetime.now(timezone.utc) - timedelta(hours=2),
              acknowledged=False),
    AlertItem(id="a2", node_id=3, alert_type="high_water",
              severity=2, message="Sump water level at 72% (above 70% warning threshold)",
              timestamp=datetime.now(timezone.utc) - timedelta(hours=5),
              acknowledged=False),
    AlertItem(id="a3", node_id=2, alert_type="low_battery",
              severity=1, message="Soil Probe North battery at 3.15V",
              timestamp=datetime.now(timezone.utc) - timedelta(days=1),
              acknowledged=True),
]

# ---------------------------------------------------------------------------
# MQTT Bridge
# ---------------------------------------------------------------------------

class MQTTBridge:
    def __init__(self):
        self.connected = False
        self.ws_clients: list[WebSocket] = []

    async def start(self):
        self.connected = True
        print("[MQTT] Bridge started")

    async def _on_message(self, topic, payload):
        data = json.loads(payload)
        for ws in self.ws_clients:
            try:
                await ws.send_json({"topic": topic, "data": data})
            except Exception:
                pass

    async def publish_command(self, user: str, command: dict):
        topic = f"stormsync/{user}/cloud/command"
        print(f"[MQTT] Publish to {topic}: {command}")

mqtt_bridge = MQTTBridge()

@app.on_event("startup")
async def startup_event():
    await mqtt_bridge.start()

# ---------------------------------------------------------------------------
# Auth
# ---------------------------------------------------------------------------

def create_jwt_token(user_id: str) -> str:
    from jose import jwt
    expire = datetime.now(timezone.utc) + timedelta(hours=JWT_EXPIRE_HOURS)
    payload = {"sub": user_id, "exp": expire}
    return jwt.encode(payload, JWT_SECRET, algorithm=JWT_ALGORITHM)

@app.post("/api/v1/auth/login", response_model=TokenResponse)
async def login(req: LoginRequest):
    if req.email == "demo@stormsync.com" and req.password == "demo":
        return TokenResponse(access_token=create_jwt_token(req.email))
    raise HTTPException(status_code=401, detail="Invalid credentials")

# ---------------------------------------------------------------------------
# Devices
# ---------------------------------------------------------------------------

@app.get("/api/v1/devices", response_model=list[DeviceInfo])
async def list_devices():
    return [DeviceInfo(**d) for d in MOCK_DEVICES]

@app.post("/api/v1/devices/{node_id}/ota")
async def trigger_ota(node_id: int, version: str = "1.1.0"):
    await mqtt_bridge.publish_command("demo", {
        "type": "ota", "node_id": node_id, "version": version,
    })
    return {"status": "initiated", "node_id": node_id, "version": version}

# ---------------------------------------------------------------------------
# Sump Pit
# ---------------------------------------------------------------------------

@app.get("/api/v1/sump", response_model=SumpReading)
async def get_sump_latest():
    return SumpReading(
        node_id=1,
        timestamp=datetime.now(timezone.utc),
        water_level_mm=350,
        water_level_pct=29.2,
        pump_current_ma=0,
        pump_status="off",
        flow_rate_lpm=0,
        water_temp_c=15.0,
        vibration_rms_mg=12,
        vibration_peak_mg=45,
        mains_power=True,
        pump_runtime_today_min=23,
        battery_v=13.2,
    )

@app.get("/api/v1/sump/history")
async def get_sump_history(hours: int = 24):
    data = []
    now = datetime.now(timezone.utc)
    for h in range(hours, 0, -1):
        ts = now - timedelta(hours=h)
        level = 300 + 80 * np.sin(h * 0.2) + np.random.normal(0, 10)
        data.append({
            "timestamp": ts.isoformat(),
            "water_level_mm": int(level),
            "water_level_pct": round(level / 12, 1),
            "pump_status": "running" if level > 350 else "off",
            "pump_current_ma": 1200 if level > 350 else 0,
        })
    return {"node_id": 1, "data_points": data}

# ---------------------------------------------------------------------------
# Soil
# ---------------------------------------------------------------------------

@app.get("/api/v1/soil", response_model=list[SoilReading])
async def get_soil_latest():
    readings = []
    for i in range(4):
        readings.append(SoilReading(
            node_id=2 + i,
            timestamp=datetime.now(timezone.utc),
            moisture_15_pct=35.0 + i * 3,
            moisture_45_pct=62.0 + i * 2,
            moisture_90_pct=78.0 + i * 2.5,
            pore_pressure_kpa=10.5 + i * 0.5,
            temp_15_c=22, temp_45_c=18, temp_90_c=15,
            battery_mv=330 - i * 5,
        ))
    return readings

# ---------------------------------------------------------------------------
# Weather
# ---------------------------------------------------------------------------

@app.get("/api/v1/weather", response_model=WeatherData)
async def get_weather():
    return WeatherData(
        timestamp=datetime.now(timezone.utc),
        temp_c=22.3, humidity_pct=58.0, pressure_hpa=1008.2,
        pressure_trend="falling",
        wind_speed_ms=3.5, wind_dir_deg=180, rain_mm=2.4,
        forecast={
            "rain_6h_mm": 15.0,
            "rain_24h_mm": 32.0,
            "flood_watch": True,
        }
    )

# ---------------------------------------------------------------------------
# Actuator
# ---------------------------------------------------------------------------

MOCK_ACTUATOR = ActuatorStatus(
    valve_status="open", pump_relay=False, float_switch=False,
    alarm_status=False, mains_power=True, battery_v=13.1,
    battery_health_pct=95
)

@app.get("/api/v1/actuator/status", response_model=ActuatorStatus)
async def get_actuator_status():
    return MOCK_ACTUATOR

@app.post("/api/v1/actuator/valve")
async def control_valve(ctrl: ValveControl):
    if ctrl.action not in ("open", "close"):
        raise HTTPException(status_code=400, detail="Invalid action")
    await mqtt_bridge.publish_command("demo", {
        "type": "valve_" + ctrl.action, "node_id": 7,
    })
    MOCK_ACTUATOR.valve_status = ctrl.action + "d" if ctrl.action == "close" else "open"
    return {"status": MOCK_ACTUATOR.valve_status,
            "timestamp": datetime.now(timezone.utc).isoformat()}

@app.post("/api/v1/actuator/pump")
async def control_pump(ctrl: PumpControl):
    if ctrl.action not in ("on", "off"):
        raise HTTPException(status_code=400, detail="Invalid action")
    await mqtt_bridge.publish_command("demo", {
        "type": "pump_" + ctrl.action, "node_id": 7,
    })
    MOCK_ACTUATOR.pump_relay = (ctrl.action == "on")
    return {"status": "running" if ctrl.action == "on" else "stopped",
            "timestamp": datetime.now(timezone.utc).isoformat()}

# ---------------------------------------------------------------------------
# Alerts
# ---------------------------------------------------------------------------

@app.get("/api/v1/alerts", response_model=list[AlertItem])
async def get_alerts(acknowledged: Optional[bool] = None):
    if acknowledged is None:
        return MOCK_ALERTS
    return [a for a in MOCK_ALERTS if a.acknowledged == acknowledged]

@app.put("/api/v1/alerts/{alert_id}/ack")
async def acknowledge_alert(alert_id: str):
    for a in MOCK_ALERTS:
        if a.id == alert_id:
            a.acknowledged = True
            return {"status": "acknowledged", "alert_id": alert_id}
    raise HTTPException(status_code=404, detail="Alert not found")

# ---------------------------------------------------------------------------
# Flood Score
# ---------------------------------------------------------------------------

@app.get("/api/v1/flood-score", response_model=FloodScore)
async def get_flood_score():
    return FloodScore(
        score=42,
        risk_level="moderate",
        confidence=0.85,
        factors={
            "sump_water_level": "normal",
            "pump_health": "bearing_wear_early",
            "soil_saturation": "elevated",
            "weather": "rain_expected_6h",
            "pressure_trend": "falling",
        },
        recommendations=[
            "Check gutters and downspouts for blockages",
            "Test backup pump (bearing wear detected on primary)",
            "Monitor forecast: 15mm rain expected in 6 hours",
            "Consider scheduling pump replacement within 3 weeks",
        ]
    )

# ---------------------------------------------------------------------------
# Pump Health
# ---------------------------------------------------------------------------

@app.get("/api/v1/pump-health", response_model=PumpHealth)
async def get_pump_health():
    return PumpHealth(
        classification="bearing_wear_early",
        confidence=0.87,
        predicted_time_to_failure_days=21,
        vibration_trend="increasing",
        current_draw_trend="stable",
        cycle_count_today=12,
        avg_cycle_duration_s=35,
        maintenance_recommended=True,
        last_maintenance="2026-01-15",
        notes="High-frequency vibration component increasing over 2-week trend. "
              "Bearing wear pattern matches class 1. Recommend replacement within 3 weeks "
              "before storm season."
    )

# ---------------------------------------------------------------------------
# Flood Forecast
# ---------------------------------------------------------------------------

@app.get("/api/v1/flood-forecast", response_model=FloodForecast)
async def get_flood_forecast():
    predictions = []
    now = datetime.now(timezone.utc)
    for t in range(24):  # 24 × 15-min = 6 hours
        ts = now + timedelta(minutes=15 * t)
        level = 350 + t * 12 + 20 * np.sin(t * 0.3)
        predictions.append({
            "timestamp": ts.isoformat(),
            "water_level_mm": int(level),
            "confidence_lower": int(level - 25),
            "confidence_upper": int(level + 25),
        })
    max_level = max(p["water_level_mm"] for p in predictions)
    max_time = predictions[max(range(24),
                               key=lambda i: predictions[i]["water_level_mm"])]["timestamp"]
    return FloodForecast(
        forecast_horizon_hours=6,
        resolution_minutes=15,
        predictions=predictions,
        max_predicted_level_mm=max_level,
        max_predicted_time=max_time,
        flood_threshold_mm=1020,
        flood_predicted=max_level >= 1020,
    )

# ---------------------------------------------------------------------------
# Water Usage
# ---------------------------------------------------------------------------

@app.get("/api/v1/water-usage", response_model=WaterUsage)
async def get_water_usage():
    daily = []
    now = datetime.now(timezone.utc)
    for d in range(7):
        ts = now - timedelta(days=d)
        daily.append({
            "date": ts.strftime("%Y-%m-%d"),
            "pump_cycles": 15 + int(np.random.normal(0, 3)),
            "runtime_min": 45 + int(np.random.normal(0, 10)),
            "estimated_liters": 800 + int(np.random.normal(0, 100)),
        })
    return WaterUsage(
        pump_cycles_today=12,
        total_pump_runtime_min=23,
        estimated_volume_liters=420,
        daily_usage=daily,
    )

# ---------------------------------------------------------------------------
# ML Predictions
# ---------------------------------------------------------------------------

@app.get("/api/v1/ml/predict/flood")
async def predict_flood():
    return {
        "risk_level": "moderate",
        "risk_score": 42,
        "time_to_flood_hours": None,
        "contributing_factors": [
            {"factor": "soil_saturation_90cm", "weight": 0.35,
             "value": "85.1%", "impact": "high"},
            {"factor": "pressure_trend", "weight": 0.25,
             "value": "falling", "impact": "moderate"},
            {"factor": "rain_forecast_6h", "weight": 0.20,
             "value": "15mm", "impact": "moderate"},
            {"factor": "pump_health", "weight": 0.15,
             "value": "bearing_wear", "impact": "low"},
            {"factor": "sump_water_level", "weight": 0.05,
             "value": "29%", "impact": "low"},
        ]
    }

@app.get("/api/v1/ml/predict/pump")
async def predict_pump_failure():
    return {
        "classification": "bearing_wear_early",
        "confidence": 0.87,
        "predicted_failure_days": 21,
        "vibration_features": {
            "rms_mg": 45.2,
            "peak_mg": 120.5,
            "kurtosis": 4.8,
            "crest_factor": 2.67,
        },
        "current_features": {
            "steady_state_ma": 1180,
            "startup_peak_ma": 3200,
            "startup_duration_ms": 180,
        },
        "recommendation": "Schedule pump replacement within 3 weeks"
    }

# ---------------------------------------------------------------------------
# WebSocket
# ---------------------------------------------------------------------------

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    mqtt_bridge.ws_clients.append(ws)
    try:
        while True:
            data = await ws.receive_text()
            # Echo/ping
            await ws.send_json({"type": "pong", "data": data})
    except WebSocketDisconnect:
        mqtt_bridge.ws_clients.remove(ws)

# ---------------------------------------------------------------------------
# Health
# ---------------------------------------------------------------------------

@app.get("/health")
async def health():
    return {"status": "ok", "service": "stormsync-api", "version": "1.0.0"}
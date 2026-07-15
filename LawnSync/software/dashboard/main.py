"""
LawnSync Cloud Dashboard — FastAPI Backend

Endpoints:
  /api/v1/auth/login          — JWT login
  /api/v1/devices             — Device list
  /api/v1/soil                — Soil readings (latest + history)
  /api/v1/weather             — Weather data
  /api/v1/irrigation/schedule — Get/update schedule
  /api/v1/irrigation/zone/{z}/run — Manual zone run
  /api/v1/scan/results        — Scan results
  /api/v1/scan/ndvi           — NDVI map
  /api/v1/alerts              — Alerts
  /api/v1/health-score        — Lawn Health Score (0-100)
  /api/v1/fertilization       — Fertilization recommendations
  /api/v1/water-usage         — Water usage stats
  /api/v1/ml/predict/disease  — Disease risk prediction
  /api/v1/ml/predict/soil     — 14-day soil moisture forecast
  /api/v1/ws                  — WebSocket real-time updates

MQTT Topics (subscribed):
  lawnsync/{user}/hub/telemetry  — Telemetry from nodes (via hub)
  lawnsync/{user}/hub/scan       — Scan results + image refs
  lawnsync/{user}/hub/status     — Hub heartbeat
"""

import asyncio
import json
import time
from datetime import datetime, timedelta, timezone
from typing import Optional

import numpy as np
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Depends, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
INFLUXDB_URL = "http://localhost:8086"
INFLUXDB_TOKEN = "lawnsync-token"
INFLUXDB_ORG = "lawnsync"
INFLUXDB_BUCKET = "lawnsync_telemetry"
POSTGRES_DSN = "postgresql://lawnsync:lawnsync@localhost:5432/lawnsync"
JWT_SECRET = "lawnsync-secret-change-in-production"
JWT_ALGORITHM = "HS256"
JWT_EXPIRE_HOURS = 24

# ---------------------------------------------------------------------------
# App Setup
# ---------------------------------------------------------------------------

app = FastAPI(
    title="LawnSync Dashboard API",
    version="1.0.0",
    description="AI-powered smart lawn & turf health management system",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Production: restrict to app domain
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
    battery_mv: int
    last_seen: Optional[datetime]
    online: bool
    firmware_version: str

class SoilReading(BaseModel):
    node_id: int
    timestamp: datetime
    moisture_pct: float
    temp_c: float
    ph: float
    nitrogen_mgkg: float
    phosphorus_mgkg: float
    potassium_mgkg: float
    light_lux: int
    battery_mv: int

class WeatherData(BaseModel):
    timestamp: datetime
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    wind_speed_ms: float
    wind_dir_deg: int
    rain_mm: float
    solar_irr_wm2: int
    uv_index: float

class IrrigationZone(BaseModel):
    zone_id: int
    name: str
    enabled: bool
    duration_min: int
    days: list[str]  # ["mon", "wed", "fri"]
    start_time: str  # "06:00"
    moisture_threshold_pct: float = 20.0
    rain_skip: bool = True
    last_run: Optional[datetime]
    next_run: Optional[datetime]

class IrrigationSchedule(BaseModel):
    zones: list[IrrigationZone]
    water_saved_liters: int
    water_saved_pct: int

class ScanResult(BaseModel):
    timestamp: datetime
    disease_class: str
    confidence: float
    avg_ndvi: float
    weed_coverage_pct: int
    dominant_weed: str
    image_url: Optional[str]
    gps_lat: Optional[float]
    gps_lon: Optional[float]

class AlertItem(BaseModel):
    id: str
    node_id: int
    alert_type: str
    severity: int
    message: str
    timestamp: datetime
    acknowledged: bool

class HealthScore(BaseModel):
    score: int
    status: str  # "Excellent", "Good", "Fair", "Poor"
    moisture_score: int
    disease_score: int
    nutrient_score: int
    density_score: int
    recommendations: list[str]

class FertilizationRec(BaseModel):
    recommended: bool
    days_until_window: int
    npk_ratio: str  # "10-10-10"
    nitrogen_lb_per_1000sqft: float
    phosphorus_lb_per_1000sqft: float
    potassium_lb_per_1000sqft: float
    notes: str

class WaterUsage(BaseModel):
    today_liters: int
    week_liters: int
    month_liters: int
    savings_vs_timer_liters: int
    savings_pct: int
    daily_usage: list[dict]  # [{date, liters}]

class DiseaseRisk(BaseModel):
    risk_level: str  # "Low", "Moderate", "High"
    risk_score: int  # 0-100
    likely_diseases: list[dict]  # [{name, probability}]
    contributing_factors: list[str]

class SoilForecast(BaseModel):
    forecast_days: int
    daily_moisture: list[dict]  # [{date, moisture_pct, confidence}]
    irrigation_recommended: bool
    next_irrigation_date: Optional[str]

# ---------------------------------------------------------------------------
# Mock Data (In production, query InfluxDB + PostgreSQL)
# ---------------------------------------------------------------------------

MOCK_DEVICES = [
    {"node_id": 0, "node_type": "hub", "name": "Hub", "battery_mv": 0,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 1, "node_type": "soil", "name": "Front Lawn - Zone 1", "battery_mv": 320,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 2, "node_type": "soil", "name": "Front Lawn - Zone 2", "battery_mv": 315,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 3, "node_type": "soil", "name": "Backyard - Zone 3", "battery_mv": 310,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 4, "node_type": "soil", "name": "Side Yard - Zone 4", "battery_mv": 305,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 5, "node_type": "weather", "name": "Weather Station", "battery_mv": 330,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 6, "node_type": "sprinkler", "name": "Sprinkler Controller", "battery_mv": 0,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
    {"node_id": 7, "node_type": "scanner", "name": "Lawn Scanner", "battery_mv": 325,
     "last_seen": datetime.now(timezone.utc), "online": True, "firmware_version": "1.0.0"},
]

DISEASE_NAMES = [
    "Healthy", "Brown Patch", "Dollar Spot", "Rust", "Fairy Ring",
    "Snow Mold", "Pythium Blight", "Necrotic Ring Spot", "Summer Patch",
    "Powdery Mildew", "Slime Mold", "Dog Spot", "Grub Damage",
    "Chinch Bug", "Sod Webworm"
]

WEED_NAMES = ["None", "Dandelion", "Crabgrass", "Clover", "Thistle",
              "Nutsedge", "Plantain", "Chickweed", "Spurge"]

# ---------------------------------------------------------------------------
# MQTT Client (stub — in production, use paho-mqtt async)
# ---------------------------------------------------------------------------

class MQTTBridge:
    """Bridges MQTT messages to WebSocket clients and InfluxDB."""
    def __init__(self):
        self.connected = False
        self.ws_clients: list[WebSocket] = []

    async def start(self):
        """Connect to MQTT broker and subscribe to topics."""
        # In production:
        # self.client = mqtt.AsyncMQTTClient(MQTT_BROKER_HOST, MQTT_BROKER_PORT)
        # await self.client.connect()
        # await self.client.subscribe("lawnsync/+/hub/telemetry")
        # await self.client.subscribe("lawnsync/+/hub/scan")
        # self.client.on_message = self._on_message
        self.connected = True
        print("[MQTT] Bridge started")

    async def _on_message(self, topic, payload):
        """Forward MQTT messages to WebSocket clients."""
        data = json.loads(payload)
        for ws in self.ws_clients:
            try:
                await ws.send_json({"topic": topic, "data": data})
            except Exception:
                pass

    async def publish_command(self, user: str, command: dict):
        """Publish a command to the hub via MQTT."""
        topic = f"lawnsync/{user}/cloud/command"
        # await self.client.publish(topic, json.dumps(command))
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
    # In production: verify against PostgreSQL users table
    if req.email == "demo@lawnsync.com" and req.password == "demo":
        token = create_jwt_token(req.email)
        return TokenResponse(access_token=token)
    raise HTTPException(status_code=401, detail="Invalid credentials")

# ---------------------------------------------------------------------------
# Devices
# ---------------------------------------------------------------------------

@app.get("/api/v1/devices", response_model=list[DeviceInfo])
async def list_devices():
    return [DeviceInfo(**d) for d in MOCK_DEVICES]

@app.post("/api/v1/devices/{node_id}/ota")
async def trigger_ota(node_id: int, version: str = "1.1.0"):
    """Trigger OTA firmware update for a specific node."""
    await mqtt_bridge.publish_command("demo", {
        "type": "ota",
        "node_id": node_id,
        "version": version,
    })
    return {"status": "initiated", "node_id": node_id, "version": version}

# ---------------------------------------------------------------------------
# Soil Data
# ---------------------------------------------------------------------------

@app.get("/api/v1/soil", response_model=list[SoilReading])
async def get_soil_latest():
    """Get latest soil readings for all nodes."""
    readings = []
    for i in range(1, 5):  # Soil nodes 1-4
        readings.append(SoilReading(
            node_id=i,
            timestamp=datetime.now(timezone.utc),
            moisture_pct=22.5 + i * 2,
            temp_c=18.0 + i * 0.5,
            ph=6.5 + i * 0.1,
            nitrogen_mgkg=35.0 + i * 3,
            phosphorus_mgkg=12.0 + i,
            potassium_mgkg=80.0 + i * 5,
            light_lux=15000 + i * 1000,
            battery_mv=320 - i * 5,
        ))
    return readings

@app.get("/api/v1/soil/history")
async def get_soil_history(node_id: int, hours: int = 24):
    """Historical soil data for a node."""
    # In production: query InfluxDB
    data = []
    now = datetime.now(timezone.utc)
    for h in range(hours, 0, -1):
        ts = now - timedelta(hours=h)
        data.append({
            "timestamp": ts.isoformat(),
            "moisture_pct": 20 + 5 * np.sin(h * 0.3) + np.random.normal(0, 1),
            "temp_c": 18 + 3 * np.sin(h * 0.2),
            "ph": 6.5 + np.random.normal(0, 0.1),
        })
    return {"node_id": node_id, "data_points": data}

# ---------------------------------------------------------------------------
# Weather
# ---------------------------------------------------------------------------

@app.get("/api/v1/weather", response_model=WeatherData)
async def get_weather():
    return WeatherData(
        timestamp=datetime.now(timezone.utc),
        temp_c=22.3,
        humidity_pct=58.0,
        pressure_hpa=1013.2,
        wind_speed_ms=3.5,
        wind_dir_deg=180,
        rain_mm=0.0,
        solar_irr_wm2=650,
        uv_index=4.2,
    )

# ---------------------------------------------------------------------------
# Irrigation
# ---------------------------------------------------------------------------

MOCK_SCHEDULE = IrrigationSchedule(
    zones=[
        IrrigationZone(zone_id=1, name="Front Lawn Left", enabled=True,
                       duration_min=15, days=["mon", "wed", "fri"],
                       start_time="06:00", moisture_threshold_pct=18.0,
                       rain_skip=True, last_run=None,
                       next_run=datetime.now(timezone.utc) + timedelta(hours=12)),
        IrrigationZone(zone_id=2, name="Front Lawn Right", enabled=True,
                       duration_min=12, days=["mon", "wed", "fri"],
                       start_time="06:15", moisture_threshold_pct=18.0,
                       rain_skip=True, last_run=None,
                       next_run=datetime.now(timezone.utc) + timedelta(hours=12)),
        IrrigationZone(zone_id=3, name="Backyard", enabled=True,
                       duration_min=20, days=["tue", "thu", "sat"],
                       start_time="05:30", moisture_threshold_pct=20.0,
                       rain_skip=True, last_run=None,
                       next_run=datetime.now(timezone.utc) + timedelta(days=1)),
        IrrigationZone(zone_id=4, name="Side Yard", enabled=False,
                       duration_min=10, days=["sun"],
                       start_time="07:00", moisture_threshold_pct=15.0,
                       rain_skip=True, last_run=None, next_run=None),
    ],
    water_saved_liters=1250,
    water_saved_pct=38,
)

@app.get("/api/v1/irrigation/schedule", response_model=IrrigationSchedule)
async def get_schedule():
    return MOCK_SCHEDULE

@app.put("/api/v1/irrigation/schedule")
async def update_schedule(schedule: IrrigationSchedule):
    """Update irrigation schedule (pushes to sprinkler controller via MQTT)."""
    await mqtt_bridge.publish_command("demo", {
        "type": "update_schedule",
        "zones": [z.model_dump() for z in schedule.zones],
    })
    return {"status": "updated", "zones": len(schedule.zones)}

@app.post("/api/v1/irrigation/zone/{zone_id}/run")
async def manual_run(zone_id: int, duration_min: int = 10):
    """Manually run a specific irrigation zone."""
    if zone_id < 1 or zone_id > 8:
        raise HTTPException(status_code=400, detail="Invalid zone ID")
    await mqtt_bridge.publish_command("demo", {
        "type": "valve_open",
        "zone": zone_id,
        "duration_s": duration_min * 60,
    })
    return {"status": "running", "zone": zone_id, "duration_min": duration_min}

# ---------------------------------------------------------------------------
# Scanner Results
# ---------------------------------------------------------------------------

@app.get("/api/v1/scan/results", response_model=list[ScanResult])
async def get_scan_results(limit: int = 10):
    results = []
    for i in range(limit):
        results.append(ScanResult(
            timestamp=datetime.now(timezone.utc) - timedelta(hours=i*24),
            disease_class=DISEASE_NAMES[1] if i == 0 else DISEASE_NAMES[0],
            confidence=0.91 if i == 0 else 0.95,
            avg_ndvi=0.65 - i * 0.02,
            weed_coverage_pct=3 + i,
            dominant_weed=WEED_NAMES[3],
            image_url=f"https://lawnsync.cloud/images/scan_{i}.jpg",
            gps_lat=37.7749,
            gps_lon=-122.4194,
        ))
    return results

@app.get("/api/v1/scan/ndvi")
async def get_ndvi_map():
    """Returns a 64×64 NDVI map (values -1 to 1)."""
    # In production: retrieve from latest scan image stored in S3
    ndvi = np.random.uniform(0.4, 0.8, (64, 64)).tolist()
    return {"width": 64, "height": 64, "ndvi_map": ndvi,
            "avg_ndvi": float(np.mean(ndvi))}

# ---------------------------------------------------------------------------
# Alerts
# ---------------------------------------------------------------------------

MOCK_ALERTS = [
    AlertItem(id="a1", node_id=1, alert_type="low_moisture",
              severity=2, message="Zone 1 soil moisture at 14% (below 18% threshold)",
              timestamp=datetime.now(timezone.utc) - timedelta(hours=2),
              acknowledged=False),
    AlertItem(id="a2", node_id=3, alert_type="disease",
              severity=3, message="Brown Patch detected in Backyard (91% confidence)",
              timestamp=datetime.now(timezone.utc) - timedelta(hours=5),
              acknowledged=False),
    AlertItem(id="a3", node_id=4, alert_type="low_battery",
              severity=1, message="Side Yard sensor battery at 3.05V",
              timestamp=datetime.now(timezone.utc) - timedelta(days=1),
              acknowledged=True),
]

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
# Health Score
# ---------------------------------------------------------------------------

@app.get("/api/v1/health-score", response_model=HealthScore)
async def get_health_score():
    return HealthScore(
        score=78,
        status="Good",
        moisture_score=72,
        disease_score=60,  # Brown patch detected
        nutrient_score=85,
        density_score=82,
        recommendations=[
            "Apply fungicide to Backyard zone to treat Brown Patch",
            "Increase irrigation for Zone 1 (moisture below threshold)",
            "Fertilize in 5 days (N levels slightly low, weather favorable)",
            "Aerate soil in 2 weeks (density score declining)",
        ],
    )

# ---------------------------------------------------------------------------
# Fertilization
# ---------------------------------------------------------------------------

@app.get("/api/v1/fertilization", response_model=FertilizationRec)
async def get_fertilization_rec():
    return FertilizationRec(
        recommended=True,
        days_until_window=5,
        npk_ratio="16-4-8",
        nitrogen_lb_per_1000sqft=0.8,
        phosphorus_lb_per_1000sqft=0.2,
        potassium_lb_per_1000sqft=0.4,
        notes="Nitrogen slightly low (32 mg/kg). Apply when soil temp > 15°C "
              "and no rain expected for 48h. Water in immediately after application.",
    )

# ---------------------------------------------------------------------------
# Water Usage
# ---------------------------------------------------------------------------

@app.get("/api/v1/water-usage", response_model=WaterUsage)
async def get_water_usage():
    daily = []
    for i in range(30):
        daily.append({
            "date": (datetime.now(timezone.utc) - timedelta(days=29-i)).strftime("%Y-%m-%d"),
            "liters": int(80 + 40 * np.sin(i * 0.3) + np.random.normal(0, 15)),
        })
    return WaterUsage(
        today_liters=95,
        week_liters=620,
        month_liters=2400,
        savings_vs_timer_liters=1250,
        savings_pct=38,
        daily_usage=daily,
    )

# ---------------------------------------------------------------------------
# ML Predictions
# ---------------------------------------------------------------------------

@app.get("/api/v1/ml/predict/disease", response_model=DiseaseRisk)
async def predict_disease_risk():
    """Disease risk prediction based on weather, soil, and scan data."""
    return DiseaseRisk(
        risk_level="Moderate",
        risk_score=45,
        likely_diseases=[
            {"name": "Brown Patch", "probability": 0.35},
            {"name": "Dollar Spot", "probability": 0.18},
            {"name": "Rust", "probability": 0.08},
        ],
        contributing_factors=[
            "High humidity (>70%) for 3+ consecutive days",
            "Soil temperature 22°C (optimal for brown patch: 20-28°C)",
            "Overnight moisture persisting past 10 AM",
            "Nitrogen level slightly low",
        ],
    )

@app.get("/api/v1/ml/predict/soil", response_model=SoilForecast)
async def predict_soil_moisture(node_id: int = 1):
    """14-day soil moisture forecast."""
    forecast = []
    now = datetime.now(timezone.utc)
    base_moisture = 22.0
    for d in range(14):
        ts = now + timedelta(days=d + 1)
        # Simulate: rain on day 3-4, drying trend otherwise
        moisture = base_moisture - d * 0.8
        if 3 <= d <= 4:
            moisture += 8.0  # Rain event
        confidence = max(0.95 - d * 0.03, 0.60)
        forecast.append({
            "date": ts.strftime("%Y-%m-%d"),
            "moisture_pct": round(max(moisture, 5.0), 1),
            "confidence": round(confidence, 2),
        })
    irrigation = forecast[0]["moisture_pct"] < 18.0
    next_irr = None
    for f in forecast:
        if f["moisture_pct"] < 18.0:
            next_irr = f["date"]
            break
    return SoilForecast(
        forecast_days=14,
        daily_moisture=forecast,
        irrigation_recommended=irrigation,
        next_irrigation_date=next_irr,
    )

# ---------------------------------------------------------------------------
# WebSocket
# ---------------------------------------------------------------------------

class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket):
        self.active.remove(ws)

    async def broadcast(self, data: dict):
        for ws in self.active:
            try:
                await ws.send_json(data)
            except Exception:
                pass

manager = ConnectionManager()

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            # Send periodic updates
            await ws.send_json({
                "type": "telemetry",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "data": {"moisture": 22.5, "temp": 18.0, "health": 78},
            })
            await asyncio.sleep(10)
    except WebSocketDisconnect:
        manager.disconnect(ws)

# ---------------------------------------------------------------------------
# Health Check
# ---------------------------------------------------------------------------

@app.get("/health")
async def health_check():
    return {"status": "ok", "service": "lawnsync-dashboard", "version": "1.0.0"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
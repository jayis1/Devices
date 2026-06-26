"""
PoolSync Dashboard — FastAPI Backend
Real-time pool chemistry, equipment control, algae forecasting, and energy optimization.
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime, timedelta
import asyncio
import json
import logging
from contextlib import asynccontextmanager

# ML model imports
from models.algae_forecast import AlgaeForecaster
from models.chem_balance import ChemBalancer
from models.clear_water import ClearWaterClassifier
from models.energy_opt import EnergyOptimizer
from models.anomaly_detect import AnomalyDetector
from models.safety_net import SafetyNetDetector

logger = logging.getLogger("poolsync")

# ============================================================
# Models (Pydantic schemas)
# ============================================================

class ChemistryReading(BaseModel):
    probe_id: int = Field(..., ge=0, le=2)
    ph: float = Field(..., ge=0, le=14)
    orp_mv: float = Field(..., ge=-2000, le=2000)
    free_cl_ppm: float = Field(..., ge=0, le=10)
    temperature_c: float = Field(..., ge=-10, le=50)
    conductivity_us: float = Field(..., ge=0, le=100000)
    turbidity_ntu: float = Field(..., ge=0, le=1000)
    timestamp: datetime

class ClarityReading(BaseModel):
    clarity_score: float = Field(..., ge=0, le=1)
    green_channel: float = Field(..., ge=0, le=1)
    turbidity_ntu: float = Field(..., ge=0, le=1000)
    algae_risk: int = Field(..., ge=0, le=3)
    image_hash: Optional[str] = None
    timestamp: datetime

class EquipmentStatus(BaseModel):
    pump_on: bool
    heater_on: bool
    pool_light_on: bool
    spa_light_on: bool
    valve1_on: bool
    valve2_on: bool
    blower_on: bool
    flow_lpm: float
    pressure_kpa: float
    current_a: float
    pump_dosing: int = Field(..., ge=0, le=3)

class DoseCommand(BaseModel):
    pump_id: int = Field(..., ge=0, le=2)  # 0=acid, 1=chlorine, 2=clarifier
    volume_ml: float = Field(..., gt=0)
    duration_s: int = Field(..., gt=0, le=600)

class EquipmentCommand(BaseModel):
    device_id: int = Field(..., ge=0, le=7)
    command: int = Field(..., ge=0, le=3)  # 0=off, 1=on, 2=toggle, 3=set_speed
    parameter: Optional[int] = None
    duration_s: Optional[int] = None

class PoolHealthScore(BaseModel):
    overall: int = Field(..., ge=0, le=100)
    chemistry: int = Field(..., ge=0, le=100)
    clarity: int = Field(..., ge=0, le=100)
    safety: int = Field(..., ge=0, le=100)
    energy: int = Field(..., ge=0, le=100)

class AlgaeForecast(BaseModel):
    risk_level: str  # "none", "low", "medium", "high"
    confidence: float = Field(..., ge=0, le=1)
    forecast_24h: float = Field(..., ge=0, le=1)
    forecast_48h: float = Field(..., ge=0, le=1)
    forecast_72h: float = Field(..., ge=0, le=1)
    contributing_factors: list[str]

# ============================================================
# Global state
# ============================================================

class PoolState:
    """In-memory pool state — backed by database in production"""
    def __init__(self):
        self.chemistry: list[ChemistryReading] = []
        self.clarity: list[ClarityReading] = []
        self.equipment: Optional[EquipmentStatus] = None
        self.solar: dict = {}
        self.alarms: list[dict] = []
        self.pool_health: PoolHealthScore = PoolHealthScore(
            overall=80, chemistry=85, clarity=90, safety=95, energy=70
        )
        self.connected_nodes: dict[str, bool] = {}

state = PoolState()

# ML models (loaded at startup)
ml_models: dict = {}

# ============================================================
# App Setup
# ============================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Load ML models on startup"""
    logger.info("Loading ML models...")
    ml_models["algae"] = AlgaeForecaster()
    ml_models["chem"] = ChemBalancer()
    ml_models["clarity"] = ClearWaterClassifier()
    ml_models["energy"] = EnergyOptimizer()
    ml_models["anomaly"] = AnomalyDetector()
    ml_models["safety"] = SafetyNetDetector()
    logger.info("ML models loaded")

    # Start background tasks
    asyncio.create_task(health_score_updater())
    asyncio.create_task(algae_forecast_updater())

    yield

    logger.info("Shutting down...")

app = FastAPI(
    title="PoolSync Dashboard",
    description="AI-powered pool & spa health intelligence — chemistry monitoring, "
                "algae forecasting, equipment control, and safety monitoring.",
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

# ============================================================
# Background Tasks
# ============================================================

async def health_score_updater():
    """Recalculate pool health score every 60 seconds"""
    while True:
        await asyncio.sleep(60)
        try:
            if state.chemistry:
                latest = state.chemistry[-1]
                # Chemistry score
                ph_score = max(0, 100 - abs(latest.ph - 7.4) * 50)
                cl_score = max(0, 100 - abs(latest.free_cl_ppm - 3.0) * 20)
                orp_score = max(0, min(100, (latest.orp_mv - 500) / 5))
                chem_score = int((ph_score + cl_score + orp_score) / 3)

                # Clarity score
                clarity_score = int(state.clarity[-1].clarity_score * 100) if state.clarity else 80

                # Safety score
                safety_score = 100
                for alarm in state.alarms[-10:]:
                    if alarm.get("severity", 0) >= 2:
                        safety_score -= 20

                # Energy score (placeholder)
                energy_score = 70

                overall = int((chem_score * 0.35 + clarity_score * 0.25 +
                               safety_score * 0.25 + energy_score * 0.15))
                state.pool_health = PoolHealthScore(
                    overall=overall,
                    chemistry=chem_score,
                    clarity=clarity_score,
                    safety=max(0, safety_score),
                    energy=energy_score,
                )
        except Exception as e:
            logger.error(f"Health score update error: {e}")

async def algae_forecast_updater():
    """Run algae forecast every 15 minutes"""
    while True:
        await asyncio.sleep(900)  # 15 minutes
        try:
            if state.chemistry and state.clarity:
                forecast = ml_models["algae"].forecast(
                    chemistry=state.chemistry,
                    clarity=state.clarity,
                )
                # Store forecast for API access
                state.algae_forecast = forecast
        except Exception as e:
            logger.error(f"Algae forecast error: {e}")

# ============================================================
# API Routes
# ============================================================

@app.get("/")
async def root():
    return {
        "system": "PoolSync",
        "version": "1.0.0",
        "description": "AI-powered pool & spa health intelligence",
        "nodes_online": sum(1 for v in state.connected_nodes.values() if v),
    }

@app.get("/health")
async def health_check():
    return {"status": "healthy", "timestamp": datetime.utcnow().isoformat()}

# --- Chemistry ---

@app.get("/api/chemistry")
async def get_chemistry(limit: int = 100, probe_id: Optional[int] = None):
    readings = state.chemistry[-limit:]
    if probe_id is not None:
        readings = [r for r in readings if r.probe_id == probe_id]
    return readings

@app.post("/api/chemistry")
async def post_chemistry(reading: ChemistryReading):
    state.chemistry.append(reading)
    # Run anomaly detection on new reading
    anomaly = ml_models["anomaly"].detect(reading)
    if anomaly:
        state.alarms.append({
            "type": "chemistry_anomaly",
            "severity": anomaly.severity,
            "message": anomaly.message,
            "timestamp": datetime.utcnow().isoformat(),
        })
    return {"status": "ok"}

@app.get("/api/chemistry/ideal")
async def get_ideal_ranges():
    return {
        "ph": {"min": 7.2, "max": 7.6, "ideal": 7.4, "unit": "pH"},
        "free_cl_ppm": {"min": 2.0, "max": 4.0, "ideal": 3.0, "unit": "ppm"},
        "orp_mv": {"min": 650, "max": 800, "ideal": 750, "unit": "mV"},
        "temperature_c": {"min": 26, "max": 30, "ideal": 28, "unit": "°C"},
        "conductivity_us": {"min": 800, "max": 2000, "ideal": 1200, "unit": "µS/cm"},
        "turbidity_ntu": {"min": 0, "max": 0.5, "ideal": 0.2, "unit": "NTU"},
    }

# --- Clarity / Camera ---

@app.get("/api/clarity")
async def get_clarity(limit: int = 50):
    return state.clarity[-limit:]

@app.post("/api/clarity")
async def post_clarity(reading: ClarityReading):
    state.clarity.append(reading)
    return {"status": "ok"}

@app.get("/api/clarity/image/{image_hash}")
async def get_clarity_image(image_hash: str):
    """Retrieve full image from cloud storage by hash"""
    # In production: fetch from S3/GCS
    return {"image_hash": image_hash, "url": f"https://storage.poolsync.io/images/{image_hash}"}

# --- Equipment ---

@app.get("/api/equipment")
async def get_equipment():
    return state.equipment or {"status": "no_data"}

@app.post("/api/equipment/command")
async def send_equipment_command(cmd: EquipmentCommand):
    """Send command to equipment controller via MQTT → Hub → Sub-GHz"""
    # In production: publish to MQTT topic for hub to relay
    return {"status": "sent", "command": cmd.dict()}

@app.post("/api/equipment/dose")
async def send_dose_command(dose: DoseCommand):
    """Send chemical dosing command"""
    # Safety checks
    max_dose = {0: 200, 1: 500, 2: 100}  # mL limits
    if dose.volume_ml > max_dose.get(dose.pump_id, 0):
        raise HTTPException(status_code=400, detail="Dose exceeds safety limit")

    # Calculate optimal dose with ML
    if state.chemistry:
        optimal = ml_models["chem"].calculate_dose(
            chemistry=state.chemistry[-1],
            pump_id=dose.pump_id,
            volume_ml=dose.volume_ml,
        )
        return {"status": "sent", "optimal_dose_ml": optimal}

    return {"status": "sent", "dose_ml": dose.volume_ml}

# --- Pool Health Score ---

@app.get("/api/health-score")
async def get_health_score():
    return state.pool_health

# --- Algae Forecast ---

@app.get("/api/algae-forecast")
async def get_algae_forecast():
    """3-day algae outbreak forecast"""
    if not state.chemistry:
        raise HTTPException(status_code=404, detail="No chemistry data available")

    forecast = ml_models["algae"].forecast(
        chemistry=state.chemistry,
        clarity=state.clarity,
    )
    return forecast

# --- Energy Optimization ---

@app.get("/api/energy/schedule")
async def get_energy_schedule():
    """Optimal pump/heater schedule based on TOU rates and solar"""
    schedule = ml_models["energy"].optimize(
        chemistry=state.chemistry,
        equipment=state.equipment,
        solar=state.solar,
    )
    return schedule

@app.get("/api/energy/usage")
async def get_energy_usage(days: int = 7):
    """Energy usage analytics for the past N days"""
    # In production: query time-series database
    return {"days": days, "total_kwh": 45.2, "avg_daily_kwh": 6.46, "cost_usd": 8.13}

# --- Alarms ---

@app.get("/api/alarms")
async def get_alarms(limit: int = 50, severity: Optional[int] = None):
    alarms = state.alarms[-limit:]
    if severity is not None:
        alarms = [a for a in alarms if a.get("severity", 0) >= severity]
    return alarms

@app.post("/api/alarms")
async def post_alarm(alarm: dict):
    state.alarms.append(alarm)
    # Push notification would be sent here
    return {"status": "ok"}

# --- Weather Integration ---

@app.get("/api/weather")
async def get_weather():
    """Current weather conditions affecting pool chemistry"""
    # In production: fetch from NWS/OWM API
    return {
        "temperature_c": 28.5,
        "humidity_pct": 65,
        "uv_index": 8,
        "rain_mm_last_24h": 0,
        "rain_mm_next_24h": 5,
        "wind_kmh": 12,
        "forecast": "Partly cloudy, light rain expected tomorrow",
    }

# --- WebSocket for Real-time Updates ---

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    try:
        while True:
            data = await websocket.receive_text()
            msg = json.loads(data)
            msg_type = msg.get("type", "")

            if msg_type == "subscribe":
                # Client subscribes to real-time updates
                pass
            elif msg_type == "chemistry":
                reading = ChemistryReading(**msg["data"])
                state.chemistry.append(reading)
            elif msg_type == "clarity":
                reading = ClarityReading(**msg["data"])
                state.clarity.append(reading)
            elif msg_type == "equipment":
                status = EquipmentStatus(**msg["data"])
                state.equipment = status
            elif msg_type == "alarm":
                state.alarms.append(msg["data"])

    except WebSocketDisconnect:
        pass

# --- Service Professional Portal ---

@app.get("/api/professional/pools")
async def get_managed_pools():
    """List pools managed by this service professional"""
    return [
        {"id": "pool-1", "name": "Smith Residence", "health_score": 85},
        {"id": "pool-2", "name": "Johnson Pool", "health_score": 72},
        {"id": "pool-3", "name": "Hotel Oasis", "health_score": 91},
    ]

@app.get("/api/professional/report/{pool_id}")
async def get_pool_report(pool_id: str):
    """Generate pool service report for service professional"""
    return {
        "pool_id": pool_id,
        "report_date": datetime.utcnow().isoformat(),
        "chemistry": state.chemistry[-1].dict() if state.chemistry else None,
        "health_score": state.pool_health.dict(),
        "algae_risk": getattr(state, "algae_forecast", {}).get("risk_level", "unknown"),
        "recommendations": [
            "Add 50mL muriatic acid to lower pH from 7.8 to 7.4",
            "Shock treatment recommended — free chlorine at 1.2 ppm (below minimum)",
            "Filter pressure elevated — backwash recommended within 24 hours",
        ],
    }

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
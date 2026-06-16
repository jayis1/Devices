"""
PowerPulse Cloud Backend — FastAPI + MQTT + TimescaleDB

Serves the REST API for the mobile app and dashboard, receives data
from hub nodes via MQTT, runs ML inference for anomaly detection and
NILM, and manages automation rules.
"""

from fastapi import FastAPI, HTTPException, Depends, Query, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import List, Optional, Dict
from datetime import datetime, timedelta
import asyncio
import json
import logging
from contextlib import asynccontextmanager

import uvicorn
from sqlalchemy import create_engine, Column, Integer, Float, String, DateTime, Boolean, BigInteger, Text
from sqlalchemy.ext.declarable import declarative_base
from sqlalchemy.orm import sessionmaker, Session
import paho.mqtt.client as mqtt

# ─── Configuration ──────────────────────────────────────────────────

DATABASE_URL = "postgresql://powerpulse:powerpulse@localhost:5432/powerpulse"
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_PREFIX = "powerpulse"

Base = declarative_base()
engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(bind=engine)

logger = logging.getLogger("powerpulse.api")

# ─── Database Models ───────────────────────────────────────────────

class CircuitReading(Base):
    """Per-circuit power readings (TimescaleDB hypertable)."""
    __tablename__ = "circuit_readings"
    id = Column(BigInteger, primary_key=True, index=True)
    timestamp = Column(DateTime, nullable=False, index=True)
    circuit_id = Column(Integer, nullable=False, index=True)
    panel_id = Column(Integer, nullable=False)
    voltage_mv = Column(Integer)
    current_ma = Column(Integer)
    power_w = Column(Integer)
    power_factor = Column(Float)
    energy_wh = Column(Integer)
    frequency_hz = Column(Float)


class ApplianceReading(Base):
    """Per-appliance tag readings (TimescaleDB hypertable)."""
    __tablename__ = "appliance_readings"
    id = Column(BigInteger, primary_key=True, index=True)
    timestamp = Column(DateTime, nullable=False, index=True)
    tag_id = Column(Integer, nullable=False, index=True)
    voltage_mv = Column(Integer)
    current_ma = Column(Integer)
    power_w = Column(Integer)
    power_factor = Column(Float)
    energy_wh = Column(BigInteger)
    relay_state = Column(Boolean)
    temperature_c = Column(Integer)


class SolarReading(Base):
    """Solar production readings (TimescaleDB hypertable)."""
    __tablename__ = "solar_readings"
    id = Column(BigInteger, primary_key=True, index=True)
    timestamp = Column(DateTime, nullable=False, index=True)
    pv_voltage_mv = Column(Integer)
    pv_current_ma = Column(Integer)
    pv_power_w = Column(Integer)
    batt_voltage_mv = Column(Integer)
    load_current_ma = Column(Integer)
    load_power_w = Column(Integer)
    soc_pct = Column(Integer)
    charge_mode = Column(Integer)
    mppt_duty_pct = Column(Integer)
    heatsink_temp_c = Column(Integer)
    fan_speed_pct = Column(Integer)
    energy_produced_wh = Column(BigInteger)
    energy_consumed_wh = Column(BigInteger)


class Alert(Base):
    """Alerts (arc faults, overloads, anomalies)."""
    __tablename__ = "alerts"
    id = Column(BigInteger, primary_key=True, index=True)
    timestamp = Column(DateTime, nullable=False, index=True)
    alert_type = Column(String(32), nullable=False, index=True)  # arc_fault, overload, anomaly
    severity = Column(Integer, nullable=False)  # 1=low, 2=medium, 3=high, 4=critical
    circuit_id = Column(Integer)  # Optional, for circuit-specific alerts
    tag_id = Column(Integer)  # Optional, for appliance-specific alerts
    message = Column(Text)
    confidence = Column(Float)  # 0-100%
    acknowledged = Column(Boolean, default=False)
    resolved = Column(Boolean, default=False)


class Device(Base):
    """Registered nodes."""
    __tablename__ = "devices"
    id = Column(Integer, primary_key=True, index=True)
    address = Column(Integer, unique=True, nullable=False, index=True)
    node_type = Column(String(32), nullable=False)  # hub, circuit_monitor, appliance_tag, solar
    name = Column(String(64))
    location = Column(String(128))
    firmware_version = Column(String(16))
    last_seen = Column(DateTime)
    online = Column(Boolean, default=False)
    battery_pct = Column(Integer)
    num_circuits = Column(Integer)
    calibrated = Column(Boolean, default=False)
    config = Column(Text)  # JSON config


class AutomationRule(Base):
    """User-defined automation rules."""
    __tablename__ = "automation_rules"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(64), nullable=False)
    enabled = Column(Boolean, default=True)
    trigger_type = Column(String(32), nullable=False)  # threshold, time, solar_production, soc
    trigger_config = Column(Text)  # JSON trigger config
    action_type = Column(String(32), nullable=False)  # relay_toggle, notify, shed_load
    action_config = Column(Text)  # JSON action config
    created_at = Column(DateTime, default=datetime.utcnow)
    last_triggered = Column(DateTime)


class EnergyBill(Base):
    """Energy billing estimation."""
    __tablename__ = "energy_bills"
    id = Column(Integer, primary_key=True, index=True)
    timestamp = Column(DateTime, nullable=False, index=True)
    period_start = Column(DateTime, nullable=False)
    period_end = Column(DateTime, nullable=False)
    total_kwh = Column(Float)
    peak_kwh = Column(Float)
    offpeak_kwh = Column(Float)
    shoulder_kwh = Column(Float)
    estimated_cost = Column(Float)
    rate_plan = Column(String(32))


# ─── Pydantic Models ────────────────────────────────────────────────

class CircuitReadingResponse(BaseModel):
    timestamp: datetime
    circuit_id: int
    voltage_mv: int
    current_ma: int
    power_w: int
    power_factor: float
    energy_wh: int

class ApplianceReadingResponse(BaseModel):
    timestamp: datetime
    tag_id: int
    voltage_mv: int
    current_ma: int
    power_w: int
    power_factor: float
    energy_wh: int
    relay_state: bool
    temperature_c: int

class SolarReadingResponse(BaseModel):
    timestamp: datetime
    pv_power_w: int
    batt_voltage_mv: int
    load_power_w: int
    soc_pct: int
    charge_mode: int
    energy_produced_wh: int
    energy_consumed_wh: int

class AlertResponse(BaseModel):
    id: int
    timestamp: datetime
    alert_type: str
    severity: int
    circuit_id: Optional[int]
    tag_id: Optional[int]
    message: str
    confidence: float
    acknowledged: bool
    resolved: bool

class DeviceResponse(BaseModel):
    id: int
    address: int
    node_type: str
    name: Optional[str]
    location: Optional[str]
    firmware_version: Optional[str]
    last_seen: Optional[datetime]
    online: bool
    battery_pct: Optional[int]

class AutomationRuleCreate(BaseModel):
    name: str
    trigger_type: str  # threshold, time, solar_production, soc
    trigger_config: Dict  # {"circuit_id": 1, "threshold_w": 3000, "operator": ">"}
    action_type: str    # relay_toggle, notify, shed_load
    action_config: Dict  # {"tag_id": 3, "relay_state": false}

class AutomationRuleResponse(BaseModel):
    id: int
    name: str
    enabled: bool
    trigger_type: str
    trigger_config: Dict
    action_type: str
    action_config: Dict
    created_at: datetime
    last_triggered: Optional[datetime]

class BillEstimateResponse(BaseModel):
    period_start: datetime
    period_end: datetime
    total_kwh: float
    estimated_cost: float
    breakdown: Dict

class PowerFlowResponse(BaseModel):
    """Real-time power flow visualization data."""
    total_consumption_w: int
    solar_production_w: int
    grid_import_w: int
    grid_export_w: int
    battery_charge_w: int
    battery_soc_pct: int
    circuit_breakdown: Dict[int, int]  # circuit_id -> watts
    appliance_breakdown: Dict[int, int]  # tag_id -> watts


# ─── FastAPI App ────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup/shutdown lifecycle."""
    # Create tables
    Base.metadata.create_all(engine)
    logger.info("Database tables created")
    
    # Connect MQTT
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()
    logger.info("MQTT client connected")
    
    yield
    
    # Shutdown
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    logger.info("Shutdown complete")


app = FastAPI(
    title="PowerPulse API",
    description="AI-powered home energy intelligence & electrical safety system",
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


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


# ─── MQTT Client ────────────────────────────────────────────────────

mqtt_client = mqtt.Client(client_id="powerpulse-api", protocol=mqtt.MQTTv311)

def on_connect(client, userdata, flags, rc):
    logger.info(f"MQTT connected with result code {rc}")
    client.subscribe(f"{MQTT_TOPIC_PREFIX}/#")

def on_message(client, userdata, msg):
    """Handle incoming MQTT messages from hub nodes."""
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode())
    except (json.JSONDecodeError, UnicodeDecodeError):
        logger.warning(f"Invalid JSON on topic {topic}")
        return
    
    db = SessionLocal()
    try:
        if "/circuit_data" in topic:
            handle_circuit_data(db, payload)
        elif "/appliance_data" in topic:
            handle_appliance_data(db, payload)
        elif "/solar_data" in topic:
            handle_solar_data(db, payload)
        elif "/arc_fault" in topic:
            handle_arc_fault(db, payload)
        elif "/overload" in topic:
            handle_overload(db, payload)
        elif "/heartbeat" in topic:
            handle_heartbeat(db, payload)
        else:
            logger.debug(f"Unhandled topic: {topic}")
    finally:
        db.close()

def handle_circuit_data(db: Session, data: dict):
    """Store circuit reading data."""
    now = datetime.utcnow()
    for reading in data.get("readings", []):
        row = CircuitReading(
            timestamp=now,
            circuit_id=reading["circuit_id"],
            panel_id=data.get("panel_id", 0),
            voltage_mv=data.get("voltage_mv", 0),
            current_ma=reading.get("current_ma", 0),
            power_w=reading.get("power_w", 0),
            power_factor=reading.get("power_factor", 0),
            energy_wh=reading.get("energy_wh", 0),
            frequency_hz=data.get("frequency_hz", 50.0),
        )
        db.add(row)
    db.commit()

def handle_appliance_data(db: Session, data: dict):
    """Store appliance tag data."""
    now = datetime.utcnow()
    row = ApplianceReading(
        timestamp=now,
        tag_id=data["tag_id"],
        voltage_mv=data.get("voltage_mv", 0),
        current_ma=data.get("current_ma", 0),
        power_w=data.get("power_w", 0),
        power_factor=data.get("power_factor", 0),
        energy_wh=data.get("energy_wh", 0),
        relay_state=data.get("relay_state", False),
        temperature_c=data.get("temperature_c", 25),
    )
    db.add(row)
    db.commit()

def handle_solar_data(db: Session, data: dict):
    """Store solar production data."""
    now = datetime.utcnow()
    row = SolarReading(
        timestamp=now,
        pv_voltage_mv=data.get("pv_voltage_mv", 0),
        pv_current_ma=data.get("pv_current_ma", 0),
        pv_power_w=data.get("pv_power_w", 0),
        batt_voltage_mv=data.get("batt_voltage_mv", 0),
        load_current_ma=data.get("load_current_ma", 0),
        load_power_w=data.get("load_power_w", 0),
        soc_pct=data.get("soc_pct", 0),
        charge_mode=data.get("charge_mode", 0),
        mppt_duty_pct=data.get("mppt_duty_pct", 0),
        heatsink_temp_c=data.get("heatsink_temp_c", 0),
        fan_speed_pct=data.get("fan_speed_pct", 0),
        energy_produced_wh=data.get("energy_produced_wh", 0),
        energy_consumed_wh=data.get("energy_consumed_wh", 0),
    )
    db.add(row)
    db.commit()

def handle_arc_fault(db: Session, data: dict):
    """Create critical arc fault alert."""
    alert = Alert(
        timestamp=datetime.utcnow(),
        alert_type="arc_fault",
        severity=data.get("severity", 4),
        circuit_id=data.get("circuit_id"),
        message=f"Arc fault detected on circuit {data.get('circuit_id', '?')}: "
                f"confidence {data.get('confidence_pct', 0)}%, "
                f"type {data.get('arc_type', 'unknown')}",
        confidence=float(data.get("confidence_pct", 0)),
    )
    db.add(alert)
    db.commit()
    logger.critical(f"ARC FAULT ALERT: {alert.message}")

def handle_overload(db: Session, data: dict):
    """Create overload alert."""
    alert = Alert(
        timestamp=datetime.utcnow(),
        alert_type="overload",
        severity=2,
        circuit_id=data.get("circuit_id"),
        message=f"Overload on circuit {data.get('circuit_id', '?')}: "
                f"{data.get('current_ma', 0) / 1000:.1f}A "
                f"(threshold {data.get('threshold_ma', 0) / 1000:.1f}A, "
                f"{data.get('overload_pct', 0)}%)",
    )
    db.add(alert)
    db.commit()

def handle_heartbeat(db: Session, data: dict):
    """Update device online status."""
    address = data.get("source_address", 0)
    device = db.query(Device).filter(Device.address == address).first()
    if device:
        device.last_seen = datetime.utcnow()
        device.online = True
        device.battery_pct = data.get("battery_pct")
        device.firmware_version = data.get("firmware_ver")
    else:
        device = Device(
            address=address,
            node_type=data.get("node_type", "unknown"),
            battery_pct=data.get("battery_pct"),
            firmware_version=data.get("firmware_ver"),
            last_seen=datetime.utcnow(),
            online=True,
        )
        db.add(device)
    db.commit()

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message


# ─── REST API Endpoints ─────────────────────────────────────────────

@app.get("/api/v1/energy/circuits", response_model=List[CircuitReadingResponse])
async def get_circuit_readings(
    circuit_id: Optional[int] = None,
    start: Optional[datetime] = None,
    end: Optional[datetime] = None,
    limit: int = Query(100, ge=1, le=10000),
    db: Session = Depends(get_db),
):
    """Get per-circuit power data (live + historical)."""
    query = db.query(CircuitReading)
    if circuit_id is not None:
        query = query.filter(CircuitReading.circuit_id == circuit_id)
    if start:
        query = query.filter(CircuitReading.timestamp >= start)
    if end:
        query = query.filter(CircuitReading.timestamp <= end)
    query = query.order_by(CircuitReading.timestamp.desc()).limit(limit)
    return query.all()


@app.get("/api/v1/energy/appliances", response_model=List[ApplianceReadingResponse])
async def get_appliance_readings(
    tag_id: Optional[int] = None,
    start: Optional[datetime] = None,
    end: Optional[datetime] = None,
    limit: int = Query(100, ge=1, le=10000),
    db: Session = Depends(get_db),
):
    """Get per-appliance tag power data."""
    query = db.query(ApplianceReading)
    if tag_id is not None:
        query = query.filter(ApplianceReading.tag_id == tag_id)
    if start:
        query = query.filter(ApplianceReading.timestamp >= start)
    if end:
        query = query.filter(ApplianceReading.timestamp <= end)
    query = query.order_by(ApplianceReading.timestamp.desc()).limit(limit)
    return query.all()


@app.get("/api/v1/energy/total", response_model=PowerFlowResponse)
async def get_total_power(db: Session = Depends(get_db)):
    """Get real-time power flow visualization data."""
    now = datetime.utcnow()
    five_min_ago = now - timedelta(minutes=5)
    
    # Latest circuit readings
    circuit_readings = db.query(CircuitReading).filter(
        CircuitReading.timestamp >= five_min_ago
    ).all()
    
    circuit_breakdown = {}
    total_consumption = 0
    for r in circuit_readings:
        if r.circuit_id not in circuit_breakdown or r.timestamp > circuit_breakdown[r.circuit_id]["ts"]:
            circuit_breakdown[r.circuit_id] = {"w": r.power_w, "ts": r.timestamp}
            total_consumption += r.power_w
    
    # Latest appliance readings
    appliance_readings = db.query(ApplianceReading).filter(
        ApplianceReading.timestamp >= five_min_ago
    ).all()
    
    appliance_breakdown = {}
    for r in appliance_readings:
        if r.tag_id not in appliance_breakdown or r.timestamp > appliance_breakdown[r.tag_id]["ts"]:
            appliance_breakdown[r.tag_id] = {"w": r.power_w, "ts": r.timestamp}
    
    # Latest solar data
    solar = db.query(SolarReading).filter(
        SolarReading.timestamp >= five_min_ago
    ).order_by(SolarReading.timestamp.desc()).first()
    
    solar_production = solar.pv_power_w if solar else 0
    battery_charge = 0
    grid_import = 0
    grid_export = 0
    
    if solar and total_consumption > 0:
        if solar_production > total_consumption:
            grid_export = solar_production - total_consumption
            battery_charge = max(0, solar_production - total_consumption - grid_export)
        else:
            grid_import = total_consumption - solar_production
    
    return PowerFlowResponse(
        total_consumption_w=total_consumption,
        solar_production_w=solar_production,
        grid_import_w=grid_import,
        grid_export_w=grid_export,
        battery_charge_w=battery_charge,
        battery_soc_pct=solar.soc_pct if solar else 0,
        circuit_breakdown={k: v["w"] for k, v in circuit_breakdown.items()},
        appliance_breakdown={k: v["w"] for k, v in appliance_breakdown.items()},
    )


@app.get("/api/v1/alerts", response_model=List[AlertResponse])
async def get_alerts(
    alert_type: Optional[str] = None,
    severity: Optional[int] = None,
    acknowledged: Optional[bool] = None,
    limit: int = Query(50, ge=1, le=1000),
    db: Session = Depends(get_db),
):
    """Get active and recent alerts."""
    query = db.query(Alert)
    if alert_type:
        query = query.filter(Alert.alert_type == alert_type)
    if severity:
        query = query.filter(Alert.severity >= severity)
    if acknowledged is not None:
        query = query.filter(Alert.acknowledged == acknowledged)
    query = query.order_by(Alert.timestamp.desc()).limit(limit)
    return query.all()


@app.post("/api/v1/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: int, db: Session = Depends(get_db)):
    """Acknowledge an alert."""
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.acknowledged = True
    db.commit()
    return {"status": "acknowledged", "alert_id": alert_id}


@app.get("/api/v1/devices", response_model=List[DeviceResponse])
async def get_devices(db: Session = Depends(get_db)):
    """List all registered nodes."""
    return db.query(Device).all()


@app.post("/api/v1/devices/{device_id}/command")
async def send_device_command(
    device_id: int,
    command: Dict,
    db: Session = Depends(get_db),
):
    """Send command to a node (relay toggle, calibration, etc.)."""
    device = db.query(Device).filter(Device.id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Publish command via MQTT
    topic = f"{MQTT_TOPIC_PREFIX}/commands/{device.address}"
    mqtt_client.publish(topic, json.dumps(command))
    
    return {"status": "sent", "device_id": device_id, "command": command}


@app.get("/api/v1/solar/production", response_model=List[SolarReadingResponse])
async def get_solar_production(
    start: Optional[datetime] = None,
    end: Optional[datetime] = None,
    limit: int = Query(100, ge=1, le=10000),
    db: Session = Depends(get_db),
):
    """Get solar production data."""
    query = db.query(SolarReading)
    if start:
        query = query.filter(SolarReading.timestamp >= start)
    if end:
        query = query.filter(SolarReading.timestamp <= end)
    query = query.order_by(SolarReading.timestamp.desc()).limit(limit)
    return query.all()


@app.get("/api/v1/solar/battery")
async def get_battery_status(db: Session = Depends(get_db)):
    """Get battery state of charge and health."""
    latest = db.query(SolarReading).order_by(SolarReading.timestamp.desc()).first()
    if not latest:
        raise HTTPException(status_code=404, detail="No solar data available")
    return {
        "voltage_mv": latest.batt_voltage_mv,
        "soc_pct": latest.soc_pct,
        "charge_mode": latest.charge_mode,
        "mppt_duty_pct": latest.mppt_duty_pct,
        "heatsink_temp_c": latest.heatsink_temp_c,
    }


@app.post("/api/v1/automation/rules", response_model=AutomationRuleResponse)
async def create_automation_rule(
    rule: AutomationRuleCreate,
    db: Session = Depends(get_db),
):
    """Create an automation rule."""
    db_rule = AutomationRule(
        name=rule.name,
        trigger_type=rule.trigger_type,
        trigger_config=json.dumps(rule.trigger_config),
        action_type=rule.action_type,
        action_config=json.dumps(rule.action_config),
    )
    db.add(db_rule)
    db.commit()
    db.refresh(db_rule)
    return AutomationRuleResponse(
        id=db_rule.id,
        name=db_rule.name,
        enabled=db_rule.enabled,
        trigger_type=db_rule.trigger_type,
        trigger_config=json.loads(db_rule.trigger_config),
        action_type=db_rule.action_type,
        action_config=json.loads(db_rule.action_config),
        created_at=db_rule.created_at,
        last_triggered=db_rule.last_triggered,
    )


@app.get("/api/v1/billing/estimate", response_model=BillEstimateResponse)
async def get_bill_estimate(db: Session = Depends(get_db)):
    """Get estimated monthly electricity bill."""
    now = datetime.utcnow()
    month_start = now.replace(day=1, hour=0, minute=0, second=0, microsecond=0)
    
    # Sum total energy this month
    readings = db.query(CircuitReading).filter(
        CircuitReading.timestamp >= month_start
    ).all()
    
    total_wh = sum(r.energy_wh for r in readings) if readings else 0
    total_kwh = total_wh / 1000.0
    
    # Simplified billing: $0.12/kWh (US average)
    estimated_cost = total_kwh * 0.12
    
    return BillEstimateResponse(
        period_start=month_start,
        period_end=now,
        total_kwh=total_kwh,
        estimated_cost=estimated_cost,
        breakdown={
            "total_kwh": total_kwh,
            "rate_per_kwh": 0.12,
            "days_remaining": (now.replace(month=now.month + 1 if now.month < 12 else 1,
                                           day=1) - now).days,
        },
    )


@app.get("/api/v1/billing/tou-schedule")
async def get_tou_schedule():
    """Get time-of-use rate schedule."""
    return {
        "plan": "TOU-DR-1",
        "seasons": {
            "summer": {
                "months": [5, 6, 7, 8, 9, 10],
                "rates": {
                    "peak": {"hours": [12, 13, 14, 15, 16, 17, 18], "rate": 0.32},
                    "offpeak": {"hours": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 22, 23], "rate": 0.14},
                    "shoulder": {"hours": [], "rate": 0.22},
                },
            },
            "winter": {
                "months": [11, 12, 1, 2, 3, 4],
                "rates": {
                    "peak": {"hours": [17, 18, 19, 20], "rate": 0.28},
                    "offpeak": {"hours": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15], "rate": 0.14},
                    "shoulder": {"hours": [16, 21, 22, 23], "rate": 0.20},
                },
            },
        },
    }


@app.post("/api/v1/billing/optimize")
async def get_load_shifting_recommendations(db: Session = Depends(get_db)):
    """Get load-shifting recommendations for TOU optimization."""
    # Get current hour
    current_hour = datetime.utcnow().hour
    
    # Get recent average power per circuit
    now = datetime.utcnow()
    day_ago = now - timedelta(days=1)
    
    recommendations = []
    
    # Check if we're in peak hours
    if current_hour >= 12 and current_hour <= 18:
        recommendations.append({
            "action": "shed",
            "appliance": "water_heater",
            "tag_id": None,
            "reason": "Peak hours — delay water heating to off-peak (9 PM)",
            "estimated_savings": 0.45,
        })
        recommendations.append({
            "action": "delay",
            "appliance": "dishwasher",
            "tag_id": None,
            "reason": "Peak hours — schedule dishwasher for off-peak",
            "estimated_savings": 0.18,
        })
        if current_hour >= 14 and current_hour <= 16:
            recommendations.append({
                "action": "shed",
                "appliance": "ev_charger",
                "tag_id": None,
                "reason": "Peak hours — pause EV charging until 9 PM",
                "estimated_savings": 1.50,
            })
    
    # Solar optimization
    latest_solar = db.query(SolarReading).order_by(SolarReading.timestamp.desc()).first()
    if latest_solar and latest_solar.pv_power_w > 500:
        recommendations.append({
            "action": "enable",
            "appliance": "water_heater",
            "tag_id": None,
            "reason": f"Solar producing {latest_solar.pv_power_w}W — good time for water heating",
            "estimated_savings": 0.30,
        })
    
    return {"recommendations": recommendations, "current_rate": "peak" if 12 <= current_hour <= 18 else "offpeak"}


@app.websocket("/ws/realtime")
async def websocket_realtime(websocket: WebSocket):
    """WebSocket for real-time power data streaming."""
    await websocket.accept()
    try:
        while True:
            # In production: subscribe to MQTT topics and forward data
            # For now: send periodic updates
            await asyncio.sleep(1)
            data = {
                "type": "power_update",
                "timestamp": datetime.utcnow().isoformat(),
                "total_watts": 0,  # Would be populated from real data
            }
            await websocket.send_json(data)
    except Exception:
        pass


# ─── Health Check ───────────────────────────────────────────────────

@app.get("/health")
async def health():
    return {"status": "ok", "service": "powerpulse-api", "version": "1.0.0"}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
"""
HearthKeep Cloud Dashboard - FastAPI Backend

Runs the MQTT broker bridge, REST API, WebSocket server,
and alert routing for the elder safety monitoring system.
"""

import os
import json
import asyncio
from datetime import datetime, timedelta
from typing import Optional, List

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
import sqlalchemy
from sqlalchemy import create_engine, Column, Integer, Float, String, Boolean, DateTime, JSON
from sqlalchemy.ext.declaratory import declarative_base
from sqlalchemy.orm import sessionmaker
import paho.mqtt.client as mqtt
import uvicorn

# ============================================================================
# CONFIGURATION
# ============================================================================

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://hearethkeep:hearethkeep@localhost:5432/hearethkeep")
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_PREFIX = "hearethkeep"

# ============================================================================
# DATABASE MODELS
# ============================================================================

Base = declarative_base()
engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(bind=engine)


class Home(Base):
    __tablename__ = "homes"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100), nullable=False)
    hub_id = Column(String(17), unique=True, nullable=False)
    address = Column(String(200))
    timezone = Column(String(50), default="UTC")
    created_at = Column(DateTime, default=datetime.utcnow)
    emergency_contacts = Column(JSON, default=list)
    alert_preferences = Column(JSON, default=dict)


class Node(Base):
    __tablename__ = "nodes"
    id = Column(Integer, primary_key=True, index=True)
    home_id = Column(Integer, sqlalchemy.ForeignKey("homes.id"))
    node_id = Column(String(17), nullable=False)
    node_type = Column(String(20), nullable=False)  # hub, room_monitor, bed_mat, wearable_tag
    room_name = Column(String(100))
    firmware_version = Column(String(20))
    battery_pct = Column(Integer)
    last_seen_at = Column(DateTime)
    online = Column(Boolean, default=False)
    created_at = Column(DateTime, default=datetime.utcnow)


class SensorReading(Base):
    __tablename__ = "sensor_readings"
    id = Column(Integer, primary_key=True, index=True)
    home_id = Column(Integer, sqlalchemy.ForeignKey("homes.id"))
    node_id = Column(String(17), nullable=False)
    reading_type = Column(String(30), nullable=False)  # radar, env, vitals, tag
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)
    data = Column(JSON, nullable=False)


class Alert(Base):
    __tablename__ = "alerts"
    id = Column(Integer, primary_key=True, index=True)
    home_id = Column(Integer, sqlalchemy.ForeignKey("homes.id"))
    node_id = Column(String(17), nullable=False)
    alert_type = Column(String(30), nullable=False)  # fall, panic, low_battery, routine_change, health_trend
    severity = Column(String(20), nullable=False)  # info, warning, urgent, emergency
    message = Column(String(500))
    acknowledged = Column(Boolean, default=False)
    resolved = Column(Boolean, default=False)
    created_at = Column(DateTime, default=datetime.utcnow, index=True)
    resolved_at = Column(DateTime)
    metadata_ = Column("metadata", JSON, default=dict)


class ActivityLog(Base):
    __tablename__ = "activity_logs"
    id = Column(Integer, primary_key=True, index=True)
    home_id = Column(Integer, sqlalchemy.ForeignKey("homes.id"))
    node_id = Column(String(17), nullable=False)
    activity_type = Column(String(50), nullable=False)  # walking, sitting, lying, cooking, bathroom, etc.
    room_name = Column(String(100))
    duration_s = Column(Integer)
    confidence = Column(Float)
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)


class WellnessMetric(Base):
    __tablename__ = "wellness_metrics"
    id = Column(Integer, primary_key=True, index=True)
    home_id = Column(Integer, sqlalchemy.ForeignKey("homes.id"))
    node_id = Column(String(17), nullable=False)
    metric_type = Column(String(30), nullable=False)  # sleep_quality, activity_level, heart_rate, breathing
    value = Column(Float)
    unit = Column(String(20))
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)


# Create tables
Base.metadata.create_all(bind=engine)

# ============================================================================
# PYDANTIC MODELS (API SCHEMAS)
# ============================================================================

class HomeCreate(BaseModel):
    name: str
    hub_id: str
    address: Optional[str] = None
    timezone: str = "UTC"
    emergency_contacts: List[dict] = []
    alert_preferences: dict = {}


class HomeResponse(BaseModel):
    id: int
    name: str
    hub_id: str
    address: Optional[str]
    timezone: str
    emergency_contacts: List[dict]
    alert_preferences: dict
    created_at: datetime

    class Config:
        from_attributes = True


class NodeCreate(BaseModel):
    home_id: int
    node_id: str
    node_type: str
    room_name: Optional[str] = None


class NodeResponse(BaseModel):
    id: int
    home_id: int
    node_id: str
    node_type: str
    room_name: Optional[str]
    firmware_version: Optional[str]
    battery_pct: Optional[int]
    last_seen_at: Optional[datetime]
    online: bool

    class Config:
        from_attributes = True


class RadarDataAPI(BaseModel):
    room: str
    presence_count: int
    position_class: str
    fall_probability: float
    movement_index: float
    distance_m: float
    velocity_ms: float
    temp: float
    humidity: float
    iaq: float
    lux: float


class BedVitalsAPI(BaseModel):
    heart_rate_bpm: float
    breathing_rate: float
    movement_index: float
    in_bed: bool
    sleep_phase: str
    hr_confidence: float
    br_confidence: float
    temp: float


class AlertCreate(BaseModel):
    home_id: int
    node_id: str
    alert_type: str
    severity: str
    message: str
    metadata: dict = {}


class AlertResponse(BaseModel):
    id: int
    home_id: int
    node_id: str
    alert_type: str
    severity: str
    message: str
    acknowledged: bool
    resolved: bool
    created_at: datetime
    resolved_at: Optional[datetime]

    class Config:
        from_attributes = True


# ============================================================================
# FASTAPI APP
# ============================================================================

app = FastAPI(
    title="HearthKeep Dashboard API",
    description="Cloud backend for the HearthKeep elder safety monitoring system",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# WebSocket connections for real-time updates
ws_connections: List[WebSocket] = []

# ============================================================================
# MQTT BRIDGE
# ============================================================================

mqtt_client = mqtt.Client(client_id="hearethkeep-api", protocol=mqtt.MQTTv311)


def on_connect(client, userdata, flags, rc):
    """Subscribe to all HearthKeep topics on connect."""
    if rc == 0:
        print(f"Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/sensors/#")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/alerts/#")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/status/#")
    else:
        print(f"MQTT connection failed: {rc}")


def on_message(client, userdata, msg):
    """Process incoming MQTT messages from hub nodes."""
    db = SessionLocal()
    try:
        topic = msg.topic
        payload = json.loads(msg.payload.decode("utf-8"))
        
        # Extract home and node info from topic
        parts = topic.split("/")
        # hearethkeep/sensors/radar/01 or hearethkeep/sensors/vitals/0A
        if len(parts) >= 4 and parts[1] == "sensors":
            sensor_type = parts[2]
            node_id = parts[3]
            
            # Store sensor reading
            reading = SensorReading(
                home_id=1,  # Default home, would look up by node_id
                node_id=node_id,
                reading_type=sensor_type,
                data=payload,
            )
            db.add(reading)
            
            # Check for alerts
            if sensor_type == "radar":
                fall_prob = payload.get("fall_prob", 0.0)
                if fall_prob > 0.85:
                    position = payload.get("position", "unknown")
                    alert = Alert(
                        home_id=1,
                        node_id=node_id,
                        alert_type="fall",
                        severity="emergency" if fall_prob > 0.95 else "urgent",
                        message=f"Fall detected in room {payload.get('room', node_id)} "
                                f"(probability: {fall_prob:.1%}, position: {position})",
                        metadata_=payload,
                    )
                    db.add(alert)
            
            elif sensor_type == "vitals":
                hr = payload.get("hr", 0)
                br = payload.get("br", 0)
                if 0 < hr < 40 or hr > 150:
                    alert = Alert(
                        home_id=1,
                        node_id=node_id,
                        alert_type="health_trend",
                        severity="urgent",
                        message=f"Abnormal heart rate: {hr:.0f} BPM",
                        metadata_=payload,
                    )
                    db.add(alert)
                if 0 < br < 8 or br > 30:
                    alert = Alert(
                        home_id=1,
                        node_id=node_id,
                        alert_type="health_trend",
                        severity="warning",
                        message=f"Abnormal breathing rate: {br:.0f} breaths/min",
                        metadata_=payload,
                    )
                    db.add(alert)
        
        elif len(parts) >= 3 and parts[1] == "alerts":
            # Direct alert from hub (fall, panic, etc.)
            alert_type = parts[2]
            alert = Alert(
                home_id=1,
                node_id=payload.get("tag_id", payload.get("room_id", "unknown")),
                alert_type=alert_type,
                severity=payload.get("severity", "urgent"),
                message=payload.get("message", f"{alert_type} alert"),
                metadata_=payload,
            )
            db.add(alert)
        
        elif len(parts) >= 3 and parts[1] == "status":
            # Node heartbeat/status
            node_id = parts[2]
            node = db.query(Node).filter(Node.node_id == node_id).first()
            if node:
                node.last_seen_at = datetime.utcnow()
                node.online = True
                node.battery_pct = payload.get("battery_pct")
                node.firmware_version = payload.get("firmware_version")
        
        db.commit()
        
        # Broadcast to WebSocket clients
        broadcast_data = {
            "topic": topic,
            "payload": payload,
            "timestamp": datetime.utcnow().isoformat(),
        }
        asyncio.run(broadcast_to_ws(broadcast_data))
        
    except Exception as e:
        print(f"Error processing MQTT message: {e}")
        db.rollback()
    finally:
        db.close()


def setup_mqtt():
    """Initialize MQTT client and connect."""
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"MQTT connection failed: {e}")


async def broadcast_to_ws(data: dict):
    """Broadcast data to all connected WebSocket clients."""
    for ws in ws_connections:
        try:
            await ws.send_json(data)
        except Exception:
            ws_connections.remove(ws)


# ============================================================================
# API ENDPOINTS
# ============================================================================

@app.on_event("startup")
async def startup():
    setup_mqtt()


@app.on_event("shutdown")
async def shutdown():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()


# --- Home Management ---

@app.post("/api/homes", response_model=HomeResponse)
def create_home(home: HomeCreate, db=Depends(lambda: SessionLocal())):
    db_home = Home(**home.dict())
    db.add(db_home)
    db.commit()
    db.refresh(db_home)
    db.close()
    return db_home


@app.get("/api/homes/{home_id}", response_model=HomeResponse)
def get_home(home_id: int, db=Depends(lambda: SessionLocal())):
    home = db.query(Home).filter(Home.id == home_id).first()
    if not home:
        raise HTTPException(status_code=404, detail="Home not found")
    db.close()
    return home


@app.get("/api/homes", response_model=List[HomeResponse])
def list_homes(db=Depends(lambda: SessionLocal())):
    homes = db.query(Home).all()
    db.close()
    return homes


# --- Node Management ---

@app.post("/api/nodes", response_model=NodeResponse)
def create_node(node: NodeCreate, db=Depends(lambda: SessionLocal())):
    db_node = Node(**node.dict())
    db.add(db_node)
    db.commit()
    db.refresh(db_node)
    db.close()
    return db_node


@app.get("/api/homes/{home_id}/nodes", response_model=List[NodeResponse])
def list_nodes(home_id: int, db=Depends(lambda: SessionLocal())):
    nodes = db.query(Node).filter(Node.home_id == home_id).all()
    db.close()
    return nodes


# --- Sensor Data ---

@app.post("/api/sensors/radar/{node_id}")
def store_radar_data(node_id: str, data: RadarDataAPI, db=Depends(lambda: SessionLocal())):
    reading = SensorReading(
        home_id=1,
        node_id=node_id,
        reading_type="radar",
        data=data.dict(),
    )
    db.add(reading)
    
    # Check for fall
    if data.fall_probability > 0.85:
        alert = Alert(
            home_id=1,
            node_id=node_id,
            alert_type="fall",
            severity="emergency" if data.fall_probability > 0.95 else "urgent",
            message=f"Fall detected (probability: {data.fall_probability:.1%}, position: {data.position_class})",
        )
        db.add(alert)
    
    db.commit()
    db.close()
    return {"status": "ok"}


@app.post("/api/sensors/vitals/{node_id}")
def store_vitals_data(node_id: str, data: BedVitalsAPI, db=Depends(lambda: SessionLocal())):
    reading = SensorReading(
        home_id=1,
        node_id=node_id,
        reading_type="vitals",
        data=data.dict(),
    )
    db.add(reading)
    
    # Check for abnormal vitals
    if data.in_bed:
        alerts = []
        if 0 < data.heart_rate_bpm < 40 or data.heart_rate_bpm > 150:
            alerts.append(Alert(
                home_id=1, node_id=node_id, alert_type="health_trend",
                severity="urgent",
                message=f"Abnormal heart rate: {data.heart_rate_bpm:.0f} BPM",
            ))
        if 0 < data.breathing_rate < 8 or data.breathing_rate > 30:
            alerts.append(Alert(
                home_id=1, node_id=node_id, alert_type="health_trend",
                severity="warning",
                message=f"Abnormal breathing: {data.breathing_rate:.0f} breaths/min",
            ))
        db.add_all(alerts)
    
    db.commit()
    db.close()
    return {"status": "ok"}


@app.get("/api/sensors/radar/{node_id}/history")
def get_radar_history(node_id: str, hours: int = 24, db=Depends(lambda: SessionLocal())):
    since = datetime.utcnow() - timedelta(hours=hours)
    readings = db.query(SensorReading).filter(
        SensorReading.node_id == node_id,
        SensorReading.reading_type == "radar",
        SensorReading.timestamp >= since,
    ).order_by(SensorReading.timestamp.desc()).limit(1000).all()
    db.close()
    return [{"timestamp": r.timestamp.isoformat(), "data": r.data} for r in readings]


@app.get("/api/sensors/vitals/{node_id}/history")
def get_vitals_history(node_id: str, hours: int = 24, db=Depends(lambda: SessionLocal())):
    since = datetime.utcnow() - timedelta(hours=hours)
    readings = db.query(SensorReading).filter(
        SensorReading.node_id == node_id,
        SensorReading.reading_type == "vitals",
        SensorReading.timestamp >= since,
    ).order_by(SensorReading.timestamp.desc()).limit(1000).all()
    db.close()
    return [{"timestamp": r.timestamp.isoformat(), "data": r.data} for r in readings]


# --- Alerts ---

@app.get("/api/alerts", response_model=List[AlertResponse])
def list_alerts(
    home_id: int = 1,
    severity: Optional[str] = None,
    acknowledged: Optional[bool] = None,
    limit: int = 50,
    db=Depends(lambda: SessionLocal()),
):
    query = db.query(Alert).filter(Alert.home_id == home_id)
    if severity:
        query = query.filter(Alert.severity == severity)
    if acknowledged is not None:
        query = query.filter(Alert.acknowledged == acknowledged)
    alerts = query.order_by(Alert.created_at.desc()).limit(limit).all()
    db.close()
    return alerts


@app.put("/api/alerts/{alert_id}/acknowledge")
def acknowledge_alert(alert_id: int, db=Depends(lambda: SessionLocal())):
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.acknowledged = True
    db.commit()
    db.close()
    return {"status": "ok"}


@app.put("/api/alerts/{alert_id}/resolve")
def resolve_alert(alert_id: int, db=Depends(lambda: SessionLocal())):
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.resolved = True
    alert.resolved_at = datetime.utcnow()
    db.commit()
    db.close()
    return {"status": "ok"}


# --- Wellness Summary ---

@app.get("/api/wellness/{home_id}")
def get_wellness_summary(home_id: int, db=Depends(lambda: SessionLocal())):
    """Get current wellness summary for a home."""
    # Get latest readings for each node
    nodes = db.query(Node).filter(Node.home_id == home_id).all()
    
    summary = {
        "home_id": home_id,
        "timestamp": datetime.utcnow().isoformat(),
        "rooms": [],
        "bed": None,
        "alerts": 0,
        "overall_status": "normal",
    }
    
    for node in nodes:
        if node.node_type == "room_monitor":
            # Get latest radar reading
            latest = db.query(SensorReading).filter(
                SensorReading.node_id == node.node_id,
                SensorReading.reading_type == "radar",
            ).order_by(SensorReading.timestamp.desc()).first()
            
            room_data = {
                "node_id": node.node_id,
                "room_name": node.room_name or f"Room {node.node_id}",
                "online": node.online,
                "battery_pct": node.battery_pct,
                "last_seen": node.last_seen_at.isoformat() if node.last_seen_at else None,
                "presence": latest.data if latest else None,
            }
            summary["rooms"].append(room_data)
        
        elif node.node_type == "bed_mat":
            latest = db.query(SensorReading).filter(
                SensorReading.node_id == node.node_id,
                SensorReading.reading_type == "vitals",
            ).order_by(SensorReading.timestamp.desc()).first()
            
            summary["bed"] = {
                "node_id": node.node_id,
                "online": node.online,
                "battery_pct": node.battery_pct,
                "last_seen": node.last_seen_at.isoformat() if node.last_seen_at else None,
                "vitals": latest.data if latest else None,
            }
    
    # Count unresolved alerts
    alert_count = db.query(Alert).filter(
        Alert.home_id == home_id,
        Alert.resolved == False,
    ).count()
    summary["alerts"] = alert_count
    
    if alert_count > 0:
        has_emergency = db.query(Alert).filter(
            Alert.home_id == home_id,
            Alert.resolved == False,
            Alert.severity == "emergency",
        ).count() > 0
        summary["overall_status"] = "emergency" if has_emergency else "alert"
    
    db.close()
    return summary


# --- WebSocket for Real-time Updates ---

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    ws_connections.append(websocket)
    try:
        while True:
            # Keep connection alive, receive commands
            data = await websocket.receive_text()
            # Process commands from client
    except WebSocketDisconnect:
        ws_connections.remove(websocket)


# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
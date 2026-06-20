"""
GreenPulse Cloud Backend — FastAPI + MQTT

Ingests per-plant telemetry, watering events, leaf scan results, and alerts
from the hub via MQTT, stores them in PostgreSQL/TimescaleDB, serves the
user + plant-care APIs, runs the disease-risk + watering-prediction engine,
and exposes a WebSocket for real-time mobile alerts.
"""

import os
import json
import asyncio
from datetime import datetime, timezone
from typing import Optional, List
from contextlib import asynccontextmanager

import paho.mqtt.client as mqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from sqlalchemy import create_engine, Column, Integer, Float, String, DateTime, Boolean, Text, BigInteger
from sqlalchemy.orm import sessionmaker, declarative_base, Session

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://green:***@db:5432/greenpulse")
MQTT_HOST    = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT    = int(os.getenv("MQTT_PORT", "1883"))
ALERT_MOISTURE_THRESHOLD = 20   # %
ALERT_DISEASE_THRESHOLD = 60     # /100
ALERT_BATTERY_THRESHOLD = 15     # %

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------
class User(Base):
    __tablename__ = "users"
    id            = Column(Integer, primary_key=True)
    name          = Column(String(120))
    home_humidity = Column(Integer, default=45)
    light_direction = Column(String(40))  # N/S/E/W
    created_at    = Column(DateTime, default=datetime.utcnow)


class Plant(Base):
    """A registered plant (paired to a tag)."""
    __tablename__ = "plants"
    id            = Column(Integer, primary_key=True)
    user_id       = Column(Integer, index=True)
    tag_id        = Column(Integer, index=True)    # mesh node ID
    name          = Column(String(120))           # user-given name
    species_id    = Column(Integer)                # species DB ID
    species_name  = Column(String(120))
    profile_id    = Column(Integer)               # care profile ID
    location      = Column(String(120))           # room name
    auto_water    = Column(Boolean, default=False)
    created_at    = Column(DateTime, default=datetime.utcnow)


class TelemetryEvent(Base):
    """Time-series: one row per 15-min telemetry from a plant tag."""
    __tablename__ = "telemetry_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    plant_id      = Column(Integer, index=True)
    tag_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    soil_moisture = Column(Integer)     # % VWC
    ambient_lux   = Column(Float)       # lux
    temp_c        = Column(Float)
    humidity_pct  = Column(Float)
    battery_pct   = Column(Integer)
    flags         = Column(Integer)


class WateringEvent(Base):
    """Watering event (auto or manual)."""
    __tablename__ = "watering_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    plant_id      = Column(Integer, index=True)
    tag_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    source        = Column(String(20))     # "auto" or "manual"
    zone          = Column(Integer)
    ml_delivered  = Column(Integer)
    duration_s    = Column(Integer)
    status        = Column(Integer)        # 0=ok 1=no_flow 2=leak 3=timeout
    pre_moisture  = Column(Integer)
    post_moisture = Column(Integer)        # filled later from next telemetry


class ScanEvent(Base):
    """Leaf scan result from the scanner."""
    __tablename__ = "scan_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    plant_id      = Column(Integer, index=True)
    tag_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    species_id    = Column(Integer)
    species_conf  = Column(Integer)
    disease_class = Column(Integer)       # 0=healthy 1=mildew 2=spot 3=rust 4=rot 5=pest
    disease_conf  = Column(Integer)
    pest_count    = Column(Integer)
    image_url     = Column(String(255))   # cloud storage URL


class PlantRiskScore(Base):
    """Per-plant risk scores computed by the prediction engine."""
    __tablename__ = "risk_scores"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    plant_id      = Column(Integer, index=True)
    tag_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    disease_risk  = Column(Integer)       # 0-100 (3-day forecast)
    water_risk    = Column(Integer)       # 0-100 (wilt risk)
    light_risk    = Column(Integer)       # 0-100 (light deficiency)
    status        = Column(Integer)       # 0=ok 1=water_soon 2=water_now 3=low_light 4=disease 5=stress
    hours_to_water = Column(Integer)      # hours until watering needed


class AlertRecord(Base):
    __tablename__ = "alerts"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    plant_id      = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    alert_type    = Column(String(40))
    severity      = Column(String(20))     # low, medium, high
    message       = Column(Text)
    acknowledged  = Column(Boolean, default=False)


# ---------------------------------------------------------------------------
# MQTT Client
# ---------------------------------------------------------------------------
mqtt_client = mqtt.Client()
mqtt_connected = False
ws_clients: dict[int, list[WebSocket]] = {}


def on_mqtt_connect(client, userdata, flags, rc):
    global mqtt_connected
    mqtt_connected = (rc == 0)
    if mqtt_connected:
        client.subscribe("greenpulse/+/plant")
        client.subscribe("greenpulse/+/watering")
        client.subscribe("greenpulse/+/scan")
        client.subscribe("greenpulse/+/risk")


def on_mqtt_message(client, userdata, msg):
    """Process incoming MQTT messages from hubs."""
    try:
        data = json.loads(msg.payload)
        db = SessionLocal()
        topic_parts = msg.topic.split("/")
        user_id = int(topic_parts[1])
        msg_type = topic_parts[2]

        if msg_type == "plant":
            tag_id = data.get("tag", 0)
            plant = db.query(Plant).filter(Plant.tag_id == tag_id,
                                            Plant.user_id == user_id).first()
            plant_id = plant.id if plant else 0

            event = TelemetryEvent(
                plant_id=plant_id, tag_id=tag_id,
                soil_moisture=data.get("soil"),
                ambient_lux=data.get("lux", 0) / 10.0,
                temp_c=data.get("temp_c", 0),
                humidity_pct=data.get("humidity", 0) / 100.0,
                battery_pct=data.get("batt", 0),
                flags=data.get("flags", 0),
            )
            db.add(event)
            check_telemetry_alerts(db, user_id, plant_id, data)

            # Compute and store risk scores
            compute_risk_scores(db, user_id, plant_id, tag_id, data)

        elif msg_type == "watering":
            tag_id = data.get("tag", 0)
            plant = db.query(Plant).filter(Plant.tag_id == tag_id).first()
            event = WateringEvent(
                plant_id=plant.id if plant else 0,
                tag_id=tag_id,
                source=data.get("source", "auto"),
                zone=data.get("zone", 0),
                ml_delivered=data.get("ml", 0),
                duration_s=data.get("duration", 0),
                status=data.get("status", 0),
                pre_moisture=data.get("pre_moisture", 0),
            )
            db.add(event)

        elif msg_type == "scan":
            tag_id = data.get("tag", 0)
            plant = db.query(Plant).filter(Plant.tag_id == tag_id).first()
            event = ScanEvent(
                plant_id=plant.id if plant else 0,
                tag_id=tag_id,
                species_id=data.get("species", 0),
                species_conf=data.get("spec_conf", 0),
                disease_class=data.get("disease", 0),
                disease_conf=data.get("dis_conf", 0),
                pest_count=data.get("pests", 0),
                image_url=data.get("image_url", ""),
            )
            db.add(event)
            if data.get("disease", 0) > 0:
                check_scan_alerts(db, user_id, plant.id if plant else 0, data)

        elif msg_type == "risk":
            tag_id = data.get("tag", 0)
            plant = db.query(Plant).filter(Plant.tag_id == tag_id).first()
            score = PlantRiskScore(
                plant_id=plant.id if plant else 0,
                tag_id=tag_id,
                disease_risk=data.get("disease_risk", 0),
                water_risk=data.get("water_risk", 0),
                light_risk=data.get("light_risk", 0),
                status=data.get("status", 0),
                hours_to_water=data.get("hours_to_water", 0xFFFF),
            )
            db.add(score)

        db.commit()
        db.close()
    except Exception as e:
        print(f"MQTT message error: {e}")


def check_telemetry_alerts(db, user_id, plant_id, data):
    alerts = []
    soil = data.get("soil", 100)
    batt = data.get("batt", 100)
    flags = data.get("flags", 0)

    if soil < ALERT_MOISTURE_THRESHOLD:
        alerts.append(("low_moisture", "high",
                        f"Soil moisture low: {soil}% — water now"))
    if flags & 0x01:
        alerts.append(("low_moisture", "high", "Moisture below species threshold"))
    if flags & 0x02:
        alerts.append(("low_light", "medium", "Light below species minimum"))
    if batt < ALERT_BATTERY_THRESHOLD:
        alerts.append(("low_battery", "low", f"Tag battery low: {batt}%"))

    for atype, sev, msg in alerts:
        alert = AlertRecord(user_id=user_id, plant_id=plant_id,
                            alert_type=atype, severity=sev, message=msg)
        db.add(alert)
        asyncio.create_task(push_ws_alert(user_id, {
            "type": atype, "severity": sev, "message": msg, "plant_id": plant_id}))


def check_scan_alerts(db, user_id, plant_id, data):
    disease = data.get("disease", 0)
    pests = data.get("pests", 0)
    if disease > 0 or pests > 0:
        disease_names = ["healthy", "powdery mildew", "leaf spot", "rust",
                         "root rot sign", "pest infestation"]
        name = disease_names[disease] if disease < 6 else "unknown"
        msg = f"{'Pests' if disease == 5 else name} detected on your plant"
        alert = AlertRecord(user_id=user_id, plant_id=plant_id,
                            alert_type="disease", severity="high", message=msg)
        db.add(alert)
        asyncio.create_task(push_ws_alert(user_id, {
            "type": "disease", "severity": "high", "message": msg,
            "plant_id": plant_id}))


def compute_risk_scores(db, user_id, plant_id, tag_id, data):
    """Compute watering-prediction + disease-risk from telemetry history."""
    # Get last 24h of telemetry for this tag
    events = db.query(TelemetryEvent).filter(
        TelemetryEvent.tag_id == tag_id
    ).order_by(TelemetryEvent.ts.desc()).limit(96).all()

    if not events:
        return

    soil = data.get("soil", 50)
    temp = data.get("temp_c", 22)
    humidity = data.get("humidity", 45) / 100.0

    # Drying rate: slope of moisture over last 24h
    if len(events) >= 4:
        recent = [e.soil_moisture for e in reversed(events[-24:])]
        dry_rate = (recent[0] - recent[-1]) / (len(recent) * 0.25)  # %/hr
    else:
        dry_rate = 0

    # Water risk
    plant = db.query(Plant).filter(Plant.tag_id == tag_id).first()
    if plant:
        from pathlib import Path
        # Simplified: use profile min_moisture from firmware table
        # In production: load from species DB
        min_moisture = 30  # default
        if dry_rate > 0 and soil > min_moisture:
            hours_to_water = int((soil - min_moisture) / dry_rate)
            water_risk = min(100, int(100 * (1 - hours_to_water / 168)))
        else:
            hours_to_water = 0
            water_risk = 100
    else:
        hours_to_water = 0xFFFF
        water_risk = 0

    # Disease risk: humidity + temp + moisture variance
    moisture_std = (sum((e.soil_moisture - soil) ** 2 for e in events[:24]) /
                    max(len(events[:24]), 1)) ** 0.5 if events else 0
    disease_risk = 0
    if humidity > 0.70:
        disease_risk += int((humidity - 0.70) * 150)
    if temp > 22:
        disease_risk += int((temp - 22) * 5)
    disease_risk += int(moisture_std * 3)
    disease_risk = min(100, max(0, disease_risk))

    # Light risk
    light_risk = 0
    if data.get("lux", 0) / 10.0 < 1000:  # very low light
        light_risk = 60

    score = PlantRiskScore(
        plant_id=plant_id, tag_id=tag_id,
        disease_risk=disease_risk, water_risk=water_risk,
        light_risk=light_risk,
        status=2 if soil < 30 else (1 if soil < 45 else 0),
        hours_to_water=hours_to_water,
    )
    db.add(score)


async def push_ws_alert(user_id: int, alert: dict):
    for ws in ws_clients.get(user_id, []):
        try:
            await ws.send_json(alert)
        except Exception:
            pass


mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message


@asynccontextmanager
async def lifespan(app: FastAPI):
    Base.metadata.create_all(engine)
    mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
    mqtt_client.loop_start()
    yield
    mqtt_client.loop_stop()
    mqtt_client.disconnect()


# ---------------------------------------------------------------------------
# FastAPI App
# ---------------------------------------------------------------------------
app = FastAPI(title="GreenPulse API", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"],
                   allow_headers=["*"])


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


@app.get("/api/v1/health")
def health():
    return {"status": "ok", "mqtt": mqtt_connected}


@app.post("/api/v1/plants")
def create_plant(user_id: int, tag_id: int, name: str, species_name: str,
                  profile_id: int, location: str = "", auto_water: bool = False,
                  db: Session = Depends(get_db)):
    plant = Plant(user_id=user_id, tag_id=tag_id, name=name,
                 species_name=species_name, profile_id=profile_id,
                 location=location, auto_water=auto_water)
    db.add(plant)
    db.commit()
    db.refresh(plant)
    return {"id": plant.id, "tag_id": plant.tag_id, "name": plant.name}


@app.get("/api/v1/plants/{user_id}")
def get_plants(user_id: int, db: Session = Depends(get_db)):
    plants = db.query(Plant).filter(Plant.user_id == user_id).all()
    result = []
    for p in plants:
        latest = db.query(TelemetryEvent).filter(
            TelemetryEvent.tag_id == p.tag_id
        ).order_by(TelemetryEvent.ts.desc()).first()
        latest_risk = db.query(PlantRiskScore).filter(
            PlantRiskScore.tag_id == p.tag_id
        ).order_by(PlantRiskScore.ts.desc()).first()
        result.append({
            "id": p.id, "tag_id": p.tag_id, "name": p.name,
            "species": p.species_name, "location": p.location,
            "auto_water": p.auto_water,
            "soil_moisture": latest.soil_moisture if latest else None,
            "battery_pct": latest.battery_pct if latest else None,
            "status": latest_risk.status if latest_risk else 0,
            "disease_risk": latest_risk.disease_risk if latest_risk else 0,
            "water_risk": latest_risk.water_risk if latest_risk else 0,
            "hours_to_water": latest_risk.hours_to_water if latest_risk else 0xFFFF,
        })
    return result


@app.get("/api/v1/plant/{plant_id}/telemetry")
def get_telemetry(plant_id: int, hours: int = 24, db: Session = Depends(get_db)):
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(404, "Plant not found")
    events = db.query(TelemetryEvent).filter(
        TelemetryEvent.tag_id == plant.tag_id
    ).order_by(TelemetryEvent.ts.desc()).limit(hours * 4).all()
    return [{"ts": e.ts.isoformat(), "soil": e.soil_moisture,
             "lux": e.ambient_lux, "temp_c": e.temp_c,
             "humidity": e.humidity_pct, "batt": e.battery_pct}
            for e in reversed(events)]


@app.get("/api/v1/plant/{plant_id}/watering")
def get_watering(plant_id: int, days: int = 30, db: Session = Depends(get_db)):
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(404, "Plant not found")
    events = db.query(WateringEvent).filter(
        WateringEvent.tag_id == plant.tag_id
    ).order_by(WateringEvent.ts.desc()).limit(days * 20).all()
    return [{"ts": e.ts.isoformat(), "source": e.source,
             "ml": e.ml_delivered, "duration_s": e.duration_s,
             "status": e.status, "pre_moisture": e.pre_moisture}
            for e in reversed(events)]


@app.get("/api/v1/plant/{plant_id}/scans")
def get_scans(plant_id: int, db: Session = Depends(get_db)):
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(404, "Plant not found")
    scans = db.query(ScanEvent).filter(
        ScanEvent.tag_id == plant.tag_id
    ).order_by(ScanEvent.ts.desc()).limit(50).all()
    disease_names = ["healthy", "powdery mildew", "leaf spot", "rust",
                     "root rot sign", "pest infestation"]
    return [{"ts": s.ts.isoformat(), "disease": disease_names[s.disease_class]
             if s.disease_class < 6 else "unknown",
             "disease_conf": s.disease_conf, "pests": s.pest_count,
             "image_url": s.image_url}
            for s in reversed(scans)]


@app.get("/api/v1/plant/{plant_id}/risk")
def get_risk(plant_id: int, days: int = 7, db: Session = Depends(get_db)):
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(404, "Plant not found")
    scores = db.query(PlantRiskScore).filter(
        PlantRiskScore.tag_id == plant.tag_id
    ).order_by(PlantRiskScore.ts.desc()).limit(days * 96).all()
    return {
        "current": {"disease_risk": scores[0].disease_risk,
                     "water_risk": scores[0].water_risk,
                     "light_risk": scores[0].light_risk,
                     "status": scores[0].status,
                     "hours_to_water": scores[0].hours_to_water} if scores else None,
        "trend": [{"ts": s.ts.isoformat(), "disease": s.disease_risk,
                   "water": s.water_risk, "light": s.light_risk}
                  for s in reversed(scores)],
    }


@app.post("/api/v1/plant/{plant_id}/water")
def trigger_watering(plant_id: int, db: Session = Depends(get_db)):
    """Send a manual watering command (via MQTT to hub → valve)."""
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(404, "Plant not found")
    # In production: publish to MQTT topic greenpulse/<user_id>/cmd/water
    mqtt_client.publish(f"greenpulse/0/cmd/water",
                        json.dumps({"tag": plant.tag_id, "duration": 30}))
    return {"status": "watering_command_sent", "tag_id": plant.tag_id}


@app.get("/api/v1/user/{uid}/alerts")
def get_alerts(uid: int, days: int = 7, db: Session = Depends(get_db)):
    alerts = db.query(AlertRecord).filter(
        AlertRecord.user_id == uid
    ).order_by(AlertRecord.ts.desc()).limit(days * 50).all()
    return [{"id": a.id, "ts": a.ts.isoformat(), "type": a.alert_type,
             "severity": a.severity, "message": a.message,
             "plant_id": a.plant_id, "acknowledged": a.acknowledged}
            for a in reversed(alerts)]


@app.post("/api/v1/scan/upload")
async def upload_scan(tag_id: int, file: UploadFile = File(...)):
    """Upload a multispectral leaf image for cloud disease/pest analysis."""
    # In production: save to S3/GCS, trigger ML pipeline, return result via MQTT
    contents = await file.read()
    # The disease CNN would process this and push results back
    return {"status": "processing", "tag_id": tag_id,
            "bytes_received": len(contents)}


@app.get("/api/v1/species/{species_id}")
def get_species_info(species_id: int, db: Session = Depends(get_db)):
    """Get species care profile from the plant database."""
    # In production: full 4000-species DB. Stub returns common profiles.
    profiles = {
        1: {"name": "Monstera deliciosa", "light": "bright indirect",
             "water": "when top 50% dry", "humidity": "40-60%",
             "min_moisture": 35, "max_moisture": 80},
        2: {"name": "Calathea orbifolia", "light": "medium indirect",
             "water": "keep moist", "humidity": "60%+",
             "min_moisture": 50, "max_moisture": 85},
        3: {"name": "Ficus lyrata (Fiddle Leaf)", "light": "very bright",
             "water": "when top dry", "humidity": "30-50%",
             "min_moisture": 30, "max_moisture": 75},
        4: {"name": "Sansevieria (Snake Plant)", "light": "low to bright",
             "water": "when fully dry", "humidity": "any",
             "min_moisture": 15, "max_moisture": 60},
        5: {"name": "Epipremnum aureum (Pothos)", "light": "low to medium",
             "water": "when top 50% dry", "humidity": "any",
             "min_moisture": 30, "max_moisture": 80},
    }
    return profiles.get(species_id, {"name": "unknown", "light": "unknown"})


@app.websocket("/api/v1/ws/alerts/{uid}")
async def ws_alerts(ws: WebSocket, uid: int):
    await ws.accept()
    ws_clients.setdefault(uid, []).append(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_clients[uid].remove(ws)
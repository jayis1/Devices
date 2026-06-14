"""
UrbanHarvest - Cloud Dashboard Backend
FastAPI + MQTT + PostgreSQL + MinIO

Receives sensor data from hub via MQTT, stores readings,
runs ML inference, serves REST API for mobile app.
"""

import os
import json
import asyncio
from datetime import datetime, timedelta
from typing import Optional, List

from fastapi import FastAPI, WebSocket, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import paho.mqtt.client as mqtt
from sqlalchemy import create_engine, Column, Integer, Float, String, DateTime, Boolean, JSON
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session
import httpx

# ========== CONFIGURATION ==========

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://urbanharvest:urbanharvest@localhost:5432/urbanharvest")
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MINIO_ENDPOINT = os.getenv("MINIO_ENDPOINT", "localhost:9000")

# ========== DATABASE ==========

engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()


class Plant(Base):
    __tablename__ = "plants"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100))
    plant_type = Column(String(50))  # tomato, basil, lettuce, etc.
    location = Column(String(100))   # balcony, grow_tent, windowsill
    sensor_node_id = Column(Integer)
    pot_size_liters = Column(Float, default=10.0)
    planted_date = Column(DateTime, default=datetime.utcnow)
    is_active = Column(Boolean, default=True)


class SensorReading(Base):
    __tablename__ = "sensor_readings"
    id = Column(Integer, primary_key=True, index=True)
    plant_id = Column(Integer, index=True)
    node_id = Column(Integer)
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)
    soil_moisture_pct = Column(Float)
    soil_ec_ms_cm = Column(Float)
    soil_temp_c = Column(Float)
    par_umol_m2s = Column(Float)
    light_lux = Column(Integer)
    leaf_wetness_pct = Column(Float)
    leaf_wetness_hours = Column(Float)
    health_index = Column(Integer)
    health_category = Column(Integer)


class WeatherReading(Base):
    __tablename__ = "weather_readings"
    id = Column(Integer, primary_key=True, index=True)
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)
    temperature_c = Column(Float)
    humidity_pct = Column(Float)
    pressure_hpa = Column(Float)
    wind_speed_kmh = Column(Float)
    wind_direction = Column(String(3))
    rain_mm = Column(Float)
    uv_index = Column(Float)
    light_lux = Column(Integer)
    battery_soc_pct = Column(Integer)
    rain_predicted = Column(Boolean)


class GrowPodStatus(Base):
    __tablename__ = "growpod_status"
    id = Column(Integer, primary_key=True, index=True)
    timestamp = Column(DateTime, default=datetime.utcnow)
    pump_running = Column(Boolean)
    nutrient_a_total_ml = Column(Float)
    nutrient_b_total_ml = Column(Float)
    ph_dose_total_ml = Column(Float)
    fan_speed_pct = Column(Integer)
    heater_on = Column(Boolean)
    humidifier_on = Column(Boolean)
    light_on = Column(Boolean)
    red_pwm = Column(Integer)
    blue_pwm = Column(Integer)
    white_pwm = Column(Integer)
    far_red_pwm = Column(Integer)
    disease_class = Column(Integer)
    disease_confidence = Column(Float)


class Alert(Base):
    __tablename__ = "alerts"
    id = Column(Integer, primary_key=True, index=True)
    plant_id = Column(Integer, index=True)
    alert_type = Column(String(50))
    severity = Column(Integer)  # 0=info, 1=warning, 2=danger, 3=critical
    message = Column(String(500))
    acknowledged = Column(Boolean, default=False)
    timestamp = Column(DateTime, default=datetime.utcnow, index=True)


class HarvestPrediction(Base):
    __tablename__ = "harvest_predictions"
    id = Column(Integer, primary_key=True, index=True)
    plant_id = Column(Integer, index=True)
    predicted_date = Column(DateTime)
    estimated_yield_g = Column(Float)
    confidence_pct = Column(Integer)
    model_version = Column(String(20))
    timestamp = Column(DateTime, default=datetime.utcnow)


class IrrigationEvent(Base):
    __tablename__ = "irrigation_events"
    id = Column(Integer, primary_key=True, index=True)
    plant_id = Column(Integer, index=True)
    volume_ml = Column(Integer)
    duration_s = Column(Integer)
    skipped = Column(Boolean, default=False)
    skip_reason = Column(String(100))
    timestamp = Column(DateTime, default=datetime.utcnow)


Base.metadata.create_all(bind=engine)

# ========== PYDANTIC MODELS ==========

class PlantCreate(BaseModel):
    name: str
    plant_type: str
    location: str
    sensor_node_id: int
    pot_size_liters: float = 10.0


class PlantResponse(BaseModel):
    id: int
    name: str
    plant_type: str
    location: str
    sensor_node_id: int
    pot_size_liters: float
    planted_date: datetime
    is_active: bool


class IrrigationCommand(BaseModel):
    plant_id: int
    volume_ml: int
    immediate: bool = True


class NutrientCommand(BaseModel):
    plant_id: int
    nutrient_a_ml: float
    nutrient_b_ml: float
    ph_adj_ml: float = 0.0


class LightCommand(BaseModel):
    pod_id: int = 1
    red: int = 80
    blue: int = 70
    white: int = 60
    far_red: int = 20


class AlertConfig(BaseModel):
    plant_id: int
    moisture_low: float = 20.0
    moisture_high: float = 80.0
    ec_max: float = 3.0
    temp_min: float = 10.0
    temp_max: float = 35.0


# ========== FASTAPI APP ==========

app = FastAPI(
    title="UrbanHarvest API",
    description="Smart urban micro-farming system — sensor data, irrigation control, disease alerts, harvest predictions",
    version="1.0.0"
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


# ========== MQTT HANDLER ==========

mqtt_client = mqtt.Client(client_id="urbanharvest-api", protocol=mqtt.MQTTv311)
latest_sensors = {}
latest_weather = {}


def on_message(client, userdata, msg):
    """Handle incoming MQTT messages from hub"""
    topic = msg.topic
    payload = json.loads(msg.payload.decode())

    if "sensors" in topic:
        plant_id = topic.split("/")[-1]
        latest_sensors[plant_id] = payload
        # Store in database
        db = SessionLocal()
        try:
            reading = SensorReading(
                plant_id=int(plant_id),
                node_id=payload.get("node_id", 0),
                soil_moisture_pct=payload.get("moisture_pct", 0),
                soil_ec_ms_cm=payload.get("ec_ms_cm", 0),
                soil_temp_c=payload.get("temp_c", 0),
                par_umol_m2s=payload.get("par_umol_m2s", 0),
                light_lux=payload.get("light_lux", 0),
                leaf_wetness_pct=payload.get("leaf_wetness_pct", 0),
                leaf_wetness_hours=payload.get("leaf_wetness_hours", 0),
                health_index=payload.get("health_index", 0),
                health_category=payload.get("health_category", 0),
            )
            db.add(reading)
            db.commit()
        except Exception as e:
            print(f"DB error: {e}")
            db.rollback()
        finally:
            db.close()

    elif "weather" in topic:
        latest_weather["current"] = payload

    elif "alerts" in topic:
        db = SessionLocal()
        try:
            alert = Alert(
                plant_id=payload.get("plant_id", 0),
                alert_type=payload.get("alert_type", "unknown"),
                severity=payload.get("severity", 0),
                message=payload.get("message", ""),
            )
            db.add(alert)
            db.commit()
        except Exception as e:
            print(f"DB error: {e}")
            db.rollback()
        finally:
            db.close()


mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.subscribe("urbanharvest/sensors/#")
mqtt_client.subscribe("urbanharvest/weather")
mqtt_client.subscribe("urbanharvest/alerts")
mqtt_client.loop_start()


# ========== API ENDPOINTS ==========

@app.get("/")
def root():
    return {
        "system": "UrbanHarvest",
        "version": "1.0.0",
        "description": "Smart urban micro-farming system",
        "endpoints": {
            "plants": "/api/plants",
            "weather": "/api/weather",
            "alerts": "/api/alerts",
            "harvest": "/api/harvest/predictions",
            "docs": "/docs"
        }
    }


@app.get("/api/plants", response_model=List[PlantResponse])
def list_plants(db: Session = Depends(get_db)):
    """List all plants with their details"""
    return db.query(Plant).filter(Plant.is_active == True).all()


@app.get("/api/plants/{plant_id}")
def get_plant_detail(plant_id: int, db: Session = Depends(get_db)):
    """Get detailed plant info with latest readings and history"""
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Plant not found")

    latest = db.query(SensorReading).filter(
        SensorReading.plant_id == plant_id
    ).order_by(SensorReading.timestamp.desc()).first()

    harvest = db.query(HarvestPrediction).filter(
        HarvestPrediction.plant_id == plant_id
    ).order_by(HarvestPrediction.timestamp.desc()).first()

    return {
        "plant": {
            "id": plant.id,
            "name": plant.name,
            "type": plant.plant_type,
            "location": plant.location,
            "planted_date": plant.planted_date.isoformat(),
            "days_growing": (datetime.utcnow() - plant.planted_date).days,
        },
        "latest_reading": {
            "soil_moisture_pct": latest.soil_moisture_pct if latest else None,
            "soil_ec_ms_cm": latest.soil_ec_ms_cm if latest else None,
            "soil_temp_c": latest.soil_temp_c if latest else None,
            "par_umol_m2s": latest.par_umol_m2s if latest else None,
            "leaf_wetness_pct": latest.leaf_wetness_pct if latest else None,
            "health_index": latest.health_index if latest else None,
            "health_category": latest.health_category if latest else None,
            "timestamp": latest.timestamp.isoformat() if latest else None,
        },
        "harvest_prediction": {
            "predicted_date": harvest.predicted_date.isoformat() if harvest else None,
            "estimated_yield_g": harvest.estimated_yield_g if harvest else None,
            "confidence_pct": harvest.confidence_pct if harvest else None,
        } if harvest else None,
    }


@app.get("/api/plants/{plant_id}/readings")
def get_plant_readings(
    plant_id: int,
    hours: int = 24,
    db: Session = Depends(get_db)
):
    """Get time-series sensor data for a plant"""
    since = datetime.utcnow() - timedelta(hours=hours)
    readings = db.query(SensorReading).filter(
        SensorReading.plant_id == plant_id,
        SensorReading.timestamp >= since
    ).order_by(SensorReading.timestamp).all()

    return [{
        "timestamp": r.timestamp.isoformat(),
        "moisture": r.soil_moisture_pct,
        "ec": r.soil_ec_ms_cm,
        "temp": r.soil_temp_c,
        "par": r.par_umol_m2s,
        "health": r.health_index,
        "leaf_wetness": r.leaf_wetness_pct,
    } for r in readings]


@app.post("/api/plants", response_model=PlantResponse)
def add_plant(plant: PlantCreate, db: Session = Depends(get_db)):
    """Register a new plant in the system"""
    db_plant = Plant(**plant.dict())
    db.add(db_plant)
    db.commit()
    db.refresh(db_plant)
    return db_plant


@app.post("/api/plants/{plant_id}/water")
def water_plant(plant_id: int, cmd: IrrigationCommand, db: Session = Depends(get_db)):
    """Send manual irrigation command to grow pod via MQTT"""
    plant = db.query(Plant).filter(Plant.id == plant_id).first()
    if not plant:
        raise HTTPException(status_code=404, detail="Plant not found")

    # Publish irrigation command to MQTT (hub → grow pod)
    mqtt_client.publish(
        "urbanharvest/growpod/cmd",
        json.dumps({
            "command": "irrigate",
            "plant_id": plant_id,
            "volume_ml": cmd.volume_ml,
            "immediate": cmd.immediate,
        })
    )

    # Log irrigation event
    event = IrrigationEvent(
        plant_id=plant_id,
        volume_ml=cmd.volume_ml,
        duration_s=cmd.volume_ml // 10,  # ~10ml/s flow
    )
    db.add(event)
    db.commit()

    return {"status": "sent", "plant_id": plant_id, "volume_ml": cmd.volume_ml}


@app.post("/api/plants/{plant_id}/nutrient")
def dose_nutrient(plant_id: int, cmd: NutrientCommand, db: Session = Depends(get_db)):
    """Send nutrient dosing command to grow pod"""
    mqtt_client.publish(
        "urbanharvest/growpod/cmd",
        json.dumps({
            "command": "nutrient",
            "plant_id": plant_id,
            "nutrient_a_ml": cmd.nutrient_a_ml,
            "nutrient_b_ml": cmd.nutrient_b_ml,
            "ph_adj_ml": cmd.ph_adj_ml,
        })
    )
    return {"status": "sent", "plant_id": plant_id}


@app.post("/api/growpod/light")
def set_light(cmd: LightCommand):
    """Control grow pod LED spectrum"""
    mqtt_client.publish(
        "urbanharvest/growpod/cmd",
        json.dumps({
            "command": "light",
            "pod_id": cmd.pod_id,
            "red": cmd.red,
            "blue": cmd.blue,
            "white": cmd.white,
            "far_red": cmd.far_red,
        })
    )
    return {"status": "sent", "spectrum": {"red": cmd.red, "blue": cmd.blue, "white": cmd.white, "far_red": cmd.far_red}}


@app.get("/api/garden/summary")
def garden_summary(db: Session = Depends(get_db)):
    """Garden-wide health dashboard"""
    plants = db.query(Plant).filter(Plant.is_active == True).all()
    total_plants = len(plants)

    # Get latest health for each plant
    health_scores = []
    alerts_count = 0
    harvest_ready = 0

    for plant in plants:
        latest = db.query(SensorReading).filter(
            SensorReading.plant_id == plant.id
        ).order_by(SensorReading.timestamp.desc()).first()

        if latest:
            health_scores.append(latest.health_index)

        plant_alerts = db.query(Alert).filter(
            Alert.plant_id == plant.id,
            Alert.acknowledged == False
        ).count()
        alerts_count += plant_alerts

        # Check if harvest is within 3 days
        harvest = db.query(HarvestPrediction).filter(
            HarvestPrediction.plant_id == plant.id
        ).order_by(HarvestPrediction.timestamp.desc()).first()

        if harvest and harvest.predicted_date <= datetime.utcnow() + timedelta(days=3):
            harvest_ready += 1

    avg_health = sum(health_scores) / len(health_scores) if health_scores else 0

    return {
        "total_plants": total_plants,
        "average_health": round(avg_health, 1),
        "active_alerts": alerts_count,
        "harvest_ready_soon": harvest_ready,
        "garden_status": "thriving" if avg_health >= 80 else "good" if avg_health >= 60 else "needs_attention",
    }


@app.get("/api/weather")
def get_weather(db: Session = Depends(get_db)):
    """Current outdoor conditions from weather station"""
    latest = db.query(WeatherReading).order_by(
        WeatherReading.timestamp.desc()
    ).first()

    if not latest:
        return {"status": "no_data"}

    return {
        "temperature_c": latest.temperature_c,
        "humidity_pct": latest.humidity_pct,
        "pressure_hpa": latest.pressure_hpa,
        "wind_speed_kmh": latest.wind_speed_kmh,
        "wind_direction": latest.wind_direction,
        "rain_mm": latest.rain_mm,
        "uv_index": latest.uv_index,
        "battery_soc_pct": latest.battery_soc_pct,
        "rain_predicted": latest.rain_predicted,
        "timestamp": latest.timestamp.isoformat(),
    }


@app.get("/api/alerts")
def get_alerts(acknowledged: bool = False, db: Session = Depends(get_db)):
    """List active alerts"""
    alerts = db.query(Alert).filter(Alert.acknowledged == acknowledged).order_by(
        Alert.timestamp.desc()
    ).limit(50).all()
    return [{
        "id": a.id,
        "plant_id": a.plant_id,
        "type": a.alert_type,
        "severity": a.severity,
        "message": a.message,
        "timestamp": a.timestamp.isoformat(),
    } for a in alerts]


@app.post("/api/alerts/{alert_id}/acknowledge")
def acknowledge_alert(alert_id: int, db: Session = Depends(get_db)):
    """Mark an alert as acknowledged"""
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.acknowledged = True
    db.commit()
    return {"status": "acknowledged"}


@app.get("/api/harvest/predictions")
def get_harvest_predictions(db: Session = Depends(get_db)):
    """Upcoming harvest dates and yield estimates"""
    predictions = db.query(HarvestPrediction).filter(
        HarvestPrediction.predicted_date >= datetime.utcnow()
    ).order_by(HarvestPrediction.predicted_date).all()

    return [{
        "plant_id": p.plant_id,
        "predicted_date": p.predicted_date.isoformat(),
        "estimated_yield_g": p.estimated_yield_g,
        "confidence_pct": p.confidence_pct,
        "days_until": (p.predicted_date - datetime.utcnow()).days,
    } for p in predictions]


@app.get("/api/planting/advice")
async def get_planting_advice(lat: float = 40.7, lon: float = -74.0):
    """What to plant now for your climate zone"""
    # Simplified: in production, use frost date API + climate zone lookup
    month = datetime.utcnow().month

    # Seasonal planting calendar (Northern Hemisphere, temperate)
    calendar = {
        1: ["microgreens", "onion_seeds_indoor"],
        2: ["microgreens", "pepper_seeds_indoor", "tomato_seeds_indoor"],
        3: ["lettuce", "spinach", "peas_outdoor", "radish"],
        4: ["lettuce", "basil_indoor", "tomato_transplant", "pepper_transplant"],
        5: ["tomato", "pepper", "cucumber", "basil", "mint"],
        6: ["tomato", "cucumber", "basil", "mint", "strawberry"],
        7: ["lettuce", "spinach", "radish", "microgreens"],
        8: ["lettuce", "spinach", "radish", "microgreens"],
        9: ["garlic", "onion_sets", "spinach", "lettuce"],
        10: ["garlic", "microgreens", "herb_propagation_indoor"],
        11: ["microgreens", "herb_propagation_indoor"],
        12: ["microgreens", "herb_propagation_indoor"],
    }

    suggestions = calendar.get(month, ["microgreens"])

    return {
        "month": month,
        "climate_zone": f"lat={lat}, lon={lon}",
        "plant_now": suggestions,
        "tip": "Start seeds indoors 6-8 weeks before last frost date for transplant crops",
    }


@app.websocket("/ws/live")
async def websocket_live(websocket: WebSocket):
    """Real-time sensor data stream"""
    await websocket.accept()
    try:
        while True:
            # Send latest sensor data every 5 seconds
            await websocket.send_json({
                "sensors": latest_sensors,
                "weather": latest_weather,
                "timestamp": datetime.utcnow().isoformat(),
            })
            await asyncio.sleep(5)
    except Exception:
        pass


# ========== STARTUP ==========

@app.on_event("startup")
def startup():
    print("UrbanHarvest API starting...")
    print(f"Database: {DATABASE_URL}")
    print(f"MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
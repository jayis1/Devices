"""
Aqua Guard — FastAPI Backend
Real-time dashboard + MQTT bridge + dosing engine
"""

from fastapi import FastAPI, WebSocket, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
import asyncio
import json
import paho.mqtt.client as mqtt
from datetime import datetime
import sqlite3
import os

app = FastAPI(title="Aqua Guard Dashboard API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
DB_PATH = os.environ.get("DB_PATH", "/data/aquaguard.db")

def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id INTEGER,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            ph REAL, temperature REAL, ammonia REAL,
            nitrite REAL, nitrate REAL, dissolved_o2 REAL,
            tds REAL, turbidity REAL, anomaly_score REAL
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS dosing_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            pump_id INTEGER, volume_ml REAL, reason TEXT
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            level TEXT, message TEXT, acknowledged BOOLEAN DEFAULT 0
        )
    """)
    conn.commit()
    conn.close()

init_db()

# ---- MQTT Bridge ----
MQTT_BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

# Latest sensor data cache (per node)
latest_data = {}

mqtt_client = mqtt.Client(client_id="aquaguard-dashboard")

def on_connect(client, userdata, flags, rc):
    print(f"MQTT connected (rc={rc})")
    client.subscribe("aquaguard/sensor_data/#")
    client.subscribe("aquaguard/feeder_status/#")
    client.subscribe("aquaguard/alerts/#")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = json.loads(msg.payload.decode())
    
    if "sensor_data" in topic:
        node_id = payload.get("node_id", 0)
        latest_data[f"sensor_{node_id}"] = payload
        # Store in DB
        conn = sqlite3.connect(DB_PATH)
        conn.execute(
            "INSERT INTO sensor_readings (node_id, ph, temperature, ammonia, nitrite, nitrate, dissolved_o2, tds, turbidity, anomaly_score) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (node_id, payload.get("ph"), payload.get("temperature"),
             payload.get("ammonia"), payload.get("nitrite"), payload.get("nitrate"),
             payload.get("dissolved_o2"), payload.get("tds"), payload.get("turbidity"),
             payload.get("anomaly_score", 0.0))
        )
        conn.commit()
        conn.close()
    elif "alerts" in topic:
        conn = sqlite3.connect(DB_PATH)
        conn.execute(
            "INSERT INTO alerts (level, message) VALUES (?, ?)",
            (payload.get("level", "INFO"), payload.get("message", ""))
        )
        conn.commit()
        conn.close()

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

@app.on_event("startup")
async def startup():
    mqtt_client.connect_async(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

# ---- WebSocket for real-time updates ----
ws_clients = []

@app.websocket("/ws/live")
async def websocket_live(websocket: WebSocket):
    await websocket.accept()
    ws_clients.append(websocket)
    try:
        while True:
            # Push latest data every second
            await websocket.send_json(latest_data)
            await asyncio.sleep(1)
    except:
        ws_clients.remove(websocket)

# ---- REST API ----

class SensorReading(BaseModel):
    node_id: int
    ph: Optional[float] = None
    temperature: Optional[float] = None
    ammonia: Optional[float] = None
    nitrite: Optional[float] = None
    nitrate: Optional[float] = None
    dissolved_o2: Optional[float] = None
    tds: Optional[float] = None
    turbidity: Optional[float] = None
    anomaly_score: Optional[float] = None

class DoseCommand(BaseModel):
    pump_id: int
    volume_ml: float
    reason: str = "manual"

class FeedCommand(BaseModel):
    portions: int

@app.get("/api/sensors/latest")
async def get_latest():
    return latest_data

@app.get("/api/sensors/history")
async def get_history(hours: int = 24, node_id: int = 1):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cutoff = datetime.utcnow().replace(hour=datetime.utcnow().hour - hours)
    rows = conn.execute(
        "SELECT * FROM sensor_readings WHERE node_id=? AND timestamp>=? ORDER BY timestamp",
        (node_id, cutoff.isoformat())
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]

@app.post("/api/dose")
async def dose(cmd: DoseCommand):
    # Send dosing command via MQTT to hub
    mqtt_client.publish("aquaguard/commands/dose", json.dumps({
        "pump_id": cmd.pump_id,
        "volume_ml": cmd.volume_ml
    }))
    # Log it
    conn = sqlite3.connect(DB_PATH)
    conn.execute("INSERT INTO dosing_log (pump_id, volume_ml, reason) VALUES (?, ?, ?)",
                 (cmd.pump_id, cmd.volume_ml, cmd.reason))
    conn.commit()
    conn.close()
    return {"status": "sent", "pump_id": cmd.pump_id, "volume_ml": cmd.volume_ml}

@app.post("/api/feed")
async def feed(cmd: FeedCommand):
    mqtt_client.publish("aquaguard/commands/feed", json.dumps({
        "portions": cmd.portions
    }))
    return {"status": "sent", "portions": cmd.portions}

@app.get("/api/alerts")
async def get_alerts(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM alerts ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]

@app.get("/api/dosing_log")
async def get_dosing_log(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM dosing_log ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]

# ---- Species Database ----

SPECIES_DB = {
    "tropical_freshwater": {
        "name": "Tropical Freshwater",
        "ranges": {
            "ph": [6.5, 7.5], "temperature": [24, 28], "ammonia": [0, 0.25],
            "nitrite": [0, 0.25], "nitrate": [0, 40], "dissolved_o2": [5, 12],
            "tds": [150, 300], "turbidity": [0, 50]
        },
        "lighting_hours": [8, 10],
        "feeding": "2-3 times daily, small portions"
    },
    "marine_reef": {
        "name": "Marine Reef",
        "ranges": {
            "ph": [8.1, 8.4], "temperature": [25, 27], "ammonia": [0, 0.02],
            "nitrite": [0, 0.02], "nitrate": [0, 5], "dissolved_o2": [6, 12],
            "tds": [30000, 35000], "turbidity": [0, 10]
        },
        "lighting_hours": [10, 12],
        "feeding": "1-2 times daily, varied diet"
    },
    "coldwater": {
        "name": "Coldwater",
        "ranges": {
            "ph": [7.0, 7.8], "temperature": [15, 20], "ammonia": [0, 0.25],
            "nitrite": [0, 0.25], "nitrate": [0, 40], "dissolved_o2": [6, 12],
            "tds": [150, 300], "turbidity": [0, 30]
        },
        "lighting_hours": [6, 8],
        "feeding": "1-2 times daily"
    }
}

@app.get("/api/species")
async def get_species():
    return SPECIES_DB

@app.get("/api/species/{tank_type}")
async def get_species_detail(tank_type: str):
    if tank_type in SPECIES_DB:
        return SPECIES_DB[tank_type]
    raise HTTPException(status_code=404, detail="Tank type not found")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
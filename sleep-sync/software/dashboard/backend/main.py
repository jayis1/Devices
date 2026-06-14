"""
SleepSync — FastAPI Backend
Real-time dashboard + MQTT bridge + sleep scoring + smart alarm
"""

from fastapi import FastAPI, WebSocket, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional, List
import asyncio
import json
import paho.mqtt.client as mqtt
from datetime import datetime, timedelta
import sqlite3
import os

app = FastAPI(title="SleepSync Dashboard API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
DB_PATH = os.environ.get("DB_PATH", "/data/sleepsync.db")


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sleep_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            heart_rate REAL,
            hrv REAL,
            resp_rate REAL,
            rrv REAL,
            movement REAL,
            snoring REAL,
            sleep_stage INTEGER,
            stage_confidence REAL,
            battery_pct REAL
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS env_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature REAL,
            humidity REAL,
            co2_ppm REAL,
            hvac_state INTEGER,
            heater_state INTEGER,
            humidifier_state INTEGER,
            errors INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS shade_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            position_pct INTEGER,
            ambient_light REAL,
            led_warm INTEGER,
            led_amber INTEGER,
            led_cool INTEGER,
            dawn_time INTEGER,
            errors INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS daily_reports (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            date DATE,
            sleep_score REAL,
            total_sleep_min REAL,
            deep_sleep_pct REAL,
            rem_sleep_pct REAL,
            light_sleep_pct REAL,
            wake_pct REAL,
            sleep_latency_min REAL,
            waso_count INTEGER,
            snoring_min REAL,
            apnea_events INTEGER,
            avg_temp REAL,
            avg_humidity REAL,
            avg_co2 REAL,
            avg_noise REAL,
            recommendations TEXT,
            PRIMARY KEY (device_id, date)
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS alarm_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            alarm_type TEXT,
            sleep_stage INTEGER,
            triggered_at DATETIME,
            snoozed BOOLEAN DEFAULT 0
        )
    """)
    conn.commit()
    conn.close()


init_db()

# ---- MQTT Bridge ----
MQTT_BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

# Latest data cache
latest_sleep = {}
latest_env = {}
latest_shade = {}

mqtt_client = mqtt.Client(client_id="sleepsync-dashboard")


def on_connect(client, userdata, flags, rc):
    print(f"MQTT connected (rc={rc})")
    client.subscribe("sleepsync/+/sleep_data")
    client.subscribe("sleepsync/+/env_data")
    client.subscribe("sleepsync/+/shade_data")
    client.subscribe("sleepsync/+/alarm")


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode())
    except Exception:
        return

    device_id = topic.split("/")[1] if len(topic.split("/")) > 1 else "default"

    if "sleep_data" in topic:
        latest_sleep[device_id] = payload
        conn = sqlite3.connect(DB_PATH)
        conn.execute(
            """INSERT INTO sleep_data
               (device_id, heart_rate, hrv, resp_rate, rrv, movement,
                snoring, sleep_stage, stage_confidence, battery_pct)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (device_id, payload.get("heart_rate"), payload.get("hrv"),
             payload.get("resp_rate"), payload.get("rrv"),
             payload.get("movement"), payload.get("snoring"),
             payload.get("sleep_stage"), payload.get("stage_confidence"),
             payload.get("battery_pct"))
        )
        conn.commit()
        conn.close()

    elif "env_data" in topic:
        latest_env[device_id] = payload
        conn = sqlite3.connect(DB_PATH)
        conn.execute(
            """INSERT INTO env_data
               (device_id, temperature, humidity, co2_ppm, hvac_state,
                heater_state, humidifier_state, errors)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (device_id, payload.get("temperature"), payload.get("humidity"),
             payload.get("co2_ppm"), payload.get("hvac_state"),
             payload.get("heater_state"), payload.get("humidifier_state"),
             payload.get("errors"))
        )
        conn.commit()
        conn.close()

    elif "shade_data" in topic:
        latest_shade[device_id] = payload
        conn = sqlite3.connect(DB_PATH)
        conn.execute(
            """INSERT INTO shade_data
               (device_id, position_pct, ambient_light, led_warm, led_amber,
                led_cool, dawn_time, errors)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (device_id, payload.get("position"), payload.get("ambient_light"),
             payload.get("led_warm"), payload.get("led_amber"),
             payload.get("led_cool"), payload.get("dawn_time"),
             payload.get("errors"))
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
            await websocket.send_json({
                "sleep": latest_sleep,
                "env": latest_env,
                "shade": latest_shade,
            })
            await asyncio.sleep(5)
    except Exception:
        ws_clients.remove(websocket)


# ---- REST API ----

class SleepDataResponse(BaseModel):
    heart_rate: Optional[float] = None
    hrv: Optional[float] = None
    resp_rate: Optional[float] = None
    movement: Optional[float] = None
    snoring: Optional[float] = None
    sleep_stage: Optional[int] = None
    sleep_stage_name: Optional[str] = None


class EnvDataResponse(BaseModel):
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    co2_ppm: Optional[float] = None
    hvac_state: Optional[int] = None


class ClimateSetpoint(BaseModel):
    temperature: float
    humidity: float


class ShadeCommand(BaseModel):
    position: int  # 0-100%


class AlarmConfig(BaseModel):
    window_start: str  # ISO time
    window_end: str
    enabled: bool = True


class SoundConfig(BaseModel):
    sound_id: int   # 0=off, 1=white, 2=pink, 3=brown, 4=rain, 5=ocean, 6=forest, 7=campfire
    volume: int     # 0-255


STAGE_NAMES = {0: "AWAKE", 1: "LIGHT", 2: "DEEP", 3: "REM"}


@app.get("/api/sleep/latest")
async def get_sleep_latest(device_id: str = "default"):
    data = latest_sleep.get(device_id, {})
    if data:
        stage = data.get("sleep_stage", 0)
        data["sleep_stage_name"] = STAGE_NAMES.get(stage, "UNKNOWN")
    return data


@app.get("/api/sleep/history")
async def get_sleep_history(hours: int = 24, device_id: str = "default"):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    rows = conn.execute(
        """SELECT * FROM sleep_data
           WHERE device_id=? AND timestamp>=?
           ORDER BY timestamp""",
        (device_id, cutoff.isoformat())
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/env/latest")
async def get_env_latest(device_id: str = "default"):
    return latest_env.get(device_id, {})


@app.get("/api/env/history")
async def get_env_history(hours: int = 24, device_id: str = "default"):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    rows = conn.execute(
        """SELECT * FROM env_data
           WHERE device_id=? AND timestamp>=?
           ORDER BY timestamp""",
        (device_id, cutoff.isoformat())
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/shade/status")
async def get_shade_status(device_id: str = "default"):
    return latest_shade.get(device_id, {})


@app.post("/api/climate/setpoint")
async def set_climate_setpoint(setpoint: ClimateSetpoint, device_id: str = "default"):
    mqtt_client.publish(
        f"sleepsync/{device_id}/commands",
        json.dumps({
            "cmd": "climate_setpoint",
            "temperature": setpoint.temperature,
            "humidity": setpoint.humidity,
        })
    )
    return {"status": "sent", "temperature": setpoint.temperature,
            "humidity": setpoint.humidity}


@app.post("/api/shade/position")
async def set_shade_position(cmd: ShadeCommand, device_id: str = "default"):
    mqtt_client.publish(
        f"sleepsync/{device_id}/commands",
        json.dumps({
            "cmd": "shade_position",
            "position": cmd.position,
        })
    )
    return {"status": "sent", "position": cmd.position}


@app.post("/api/alarm")
async def set_alarm(config: AlarmConfig, device_id: str = "default"):
    mqtt_client.publish(
        f"sleepsync/{device_id}/commands",
        json.dumps({
            "cmd": "alarm_set",
            "window_start": config.window_start,
            "window_end": config.window_end,
            "enabled": config.enabled,
        })
    )
    return {"status": "sent", "window_start": config.window_start,
            "window_end": config.window_end}


@app.post("/api/sound")
async def set_sound(config: SoundConfig, device_id: str = "default"):
    mqtt_client.publish(
        f"sleepsync/{device_id}/commands",
        json.dumps({
            "cmd": "sound_set",
            "sound_id": config.sound_id,
            "volume": config.volume,
        })
    )
    return {"status": "sent", "sound_id": config.sound_id,
            "volume": config.volume}


@app.get("/api/sleep/score")
async def get_sleep_score(device_id: str = "default"):
    """Compute current sleep score from last night's data"""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cutoff = datetime.utcnow() - timedelta(hours=12)

    rows = conn.execute(
        """SELECT sleep_stage FROM sleep_data
           WHERE device_id=? AND timestamp>=?
           ORDER BY timestamp""",
        (device_id, cutoff.isoformat())
    ).fetchall()
    conn.close()

    if not rows:
        return {"score": 0, "stages": {}}

    stages = [r["sleep_stage"] for r in rows if r["sleep_stage"] is not None]
    total = len(stages)
    if total == 0:
        return {"score": 0, "stages": {}}

    deep_pct = sum(1 for s in stages if s == 2) / total * 100
    rem_pct = sum(1 for s in stages if s == 3) / total * 100
    wake_pct = sum(1 for s in stages if s == 0) / total * 100

    # Scoring (same logic as hub firmware)
    score = 70.0
    if 15 <= deep_pct <= 25:
        score += 15
    elif deep_pct >= 10:
        score += 8
    else:
        score -= 5

    if 20 <= rem_pct <= 30:
        score += 10
    elif rem_pct >= 15:
        score += 5
    else:
        score -= 5

    if wake_pct < 5:
        score += 5
    elif wake_pct > 15:
        score -= 10

    score = max(0, min(100, score))

    return {
        "score": round(score, 1),
        "total_samples": total,
        "stages": {
            "deep_pct": round(deep_pct, 1),
            "rem_pct": round(rem_pct, 1),
            "light_pct": round(sum(1 for s in stages if s == 1) / total * 100, 1),
            "wake_pct": round(wake_pct, 1),
        }
    }


@app.get("/api/report/daily")
async def get_daily_report(date: Optional[str] = None, device_id: str = "default"):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    if date:
        row = conn.execute(
            "SELECT * FROM daily_reports WHERE device_id=? AND date=?",
            (device_id, date)
        ).fetchone()
    else:
        row = conn.execute(
            "SELECT * FROM daily_reports WHERE device_id=? ORDER BY date DESC LIMIT 1",
            (device_id,)
        ).fetchone()
    conn.close()

    if row:
        return dict(row)
    return {"message": "No report available yet"}


# ---- Environment Optimizer Recommendations ----

POPULATION_OPTIMAL = {
    "temperature": {"min": 18.3, "max": 20.0, "unit": "°C"},
    "humidity": {"min": 40.0, "max": 50.0, "unit": "%RH"},
    "co2": {"min": 0, "max": 800, "unit": "ppm"},
    "light": {"min": 0, "max": 1, "unit": "lux"},
    "noise": {"min": 0, "max": 30, "unit": "dBA"},
}


@app.get("/api/env/recommendations")
async def get_env_recommendations(device_id: str = "default"):
    """Return population-level + personalized environment recommendations"""
    return {
        "population_optimal": POPULATION_OPTIMAL,
        "personalized": None,  # Will be populated by ML pipeline
        "note": "Personalized recommendations appear after 7 nights of data"
    }


@app.get("/api/health/apnea_risk")
async def get_apnea_risk(device_id: str = "default"):
    """Calculate apnea risk from recent snoring data"""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cutoff = datetime.utcnow() - timedelta(days=7)
    rows = conn.execute(
        """SELECT AVG(snoring) as avg_snoring, COUNT(*) as samples,
                  SUM(CASE WHEN snoring > 100 THEN 1 ELSE 0 END) as snore_events
           FROM sleep_data
           WHERE device_id=? AND timestamp>=?""",
        (device_id, cutoff.isoformat())
    ).fetchone()
    conn.close()

    if not rows or rows["samples"] == 0:
        return {"risk": "unknown", "ahi_estimate": 0, "recommendation": "Need more data"}

    avg_snoring = rows["avg_snoring"] or 0
    snore_events = rows["snore_events"] or 0
    total_hours = rows["samples"] * 5 / 3600  # 5s per sample

    ahi_estimate = snore_events / total_hours if total_hours > 0 else 0

    if ahi_estimate < 5:
        risk = "low"
        rec = "No significant apnea indicators detected"
    elif ahi_estimate < 15:
        risk = "mild"
        rec = "Mild apnea indicators detected. Consider clinical evaluation."
    elif ahi_estimate < 30:
        risk = "moderate"
        rec = "Moderate apnea indicators. Clinical evaluation recommended."
    else:
        risk = "severe"
        rec = "Significant apnea indicators. Clinical evaluation strongly recommended."

    return {
        "risk": risk,
        "ahi_estimate": round(ahi_estimate, 1),
        "avg_snoring_intensity": round(avg_snoring, 1),
        "snoring_events_7d": snore_events,
        "recommendation": rec,
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
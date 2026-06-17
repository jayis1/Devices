"""
WashWise — FastAPI Backend
Real-time laundry dashboard + MQTT bridge + fire safety alerting + dose optimization
"""

from fastapi import FastAPI, WebSocket, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional, List
import asyncio
import json
import paho.mqtt.client as mqtt
from datetime import datetime
import sqlite3
import os

app = FastAPI(title="WashWise Dashboard API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
DB_PATH = os.environ.get("DB_PATH", "/data/washwise.db")


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS washer_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            cycle_phase INTEGER, vibration_rms REAL, flow_rate REAL,
            total_water_ml REAL, water_temp REAL, ambient_humidity REAL,
            motor_state INTEGER, current_ma REAL, detergent_mg REAL,
            reservoir_g REAL, fabric_type INTEGER, imbalance_flag INTEGER,
            leak_flag INTEGER, battery_pct INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS dryer_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            exhaust_temp REAL, ambient_temp REAL, diff_pressure REAL,
            exhaust_humidity REAL, vibration_rms REAL, current_ma REAL,
            dryer_state INTEGER, heating_on INTEGER, fire_risk_score REAL,
            lint_clog_level INTEGER, dryness_level INTEGER,
            alert_level INTEGER, battery_pct INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS scan_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            fabric_type INTEGER, fabric_confidence REAL,
            stain_type INTEGER, stain_confidence REAL,
            wash_temp REAL, recommended_cycle INTEGER,
            detergent_ml INTEGER, pre_treat_id INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            level TEXT, source TEXT, message TEXT, acknowledged BOOLEAN DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS energy_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            node_type TEXT, cycle_energy_wh REAL, cycle_water_ml REAL,
            cycle_duration_s REAL, estimated_cost_cents REAL, co2_g REAL
        )
    """)
    conn.commit()
    conn.close()


init_db()

# ---- MQTT Bridge ----
MQTT_BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

latest_data = {
    "washer": {},
    "dryer": {},
    "scanner": {},
    "fire_risk": 0.0,
    "dryer_active": False,
}

mqtt_client = mqtt.Client(client_id="washwise-dashboard")


def on_connect(client, userdata, flags, rc):
    print(f"MQTT connected (rc={rc})")
    client.subscribe("washwise/washer_data")
    client.subscribe("washwise/dryer_data")
    client.subscribe("washwise/scan_result")
    client.subscribe("washwise/fire_alert")
    client.subscribe("washwise/energy_data")
    client.subscribe("washwise/alerts")


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        return

    conn = sqlite3.connect(DB_PATH)

    if topic == "washwise/washer_data":
        latest_data["washer"] = payload
        conn.execute(
            """INSERT INTO washer_readings
               (cycle_phase, vibration_rms, flow_rate, total_water_ml,
                water_temp, ambient_humidity, motor_state, current_ma,
                detergent_mg, reservoir_g, fabric_type, imbalance_flag,
                leak_flag, battery_pct)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)""",
            (payload.get("cycle_phase"), payload.get("vibration_rms"),
             payload.get("flow_rate"), payload.get("total_water_ml"),
             payload.get("water_temp"), payload.get("ambient_humidity"),
             payload.get("motor_state"), payload.get("current_ma"),
             payload.get("detergent_mg"), payload.get("reservoir_g"),
             payload.get("fabric_type"), payload.get("imbalance_flag"),
             payload.get("leak_flag"), payload.get("battery_pct")),
        )
        # Leak alert
        if payload.get("leak_flag", 0) >= 1:
            level = "WARNING" if payload["leak_flag"] == 1 else "CRITICAL"
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                (level, "washer", "Possible washer leak detected"),
            )

    elif topic == "washwise/dryer_data":
        latest_data["dryer"] = payload
        latest_data["fire_risk"] = payload.get("fire_risk_score", 0) / 255.0
        latest_data["dryer_active"] = payload.get("dryer_state", 0) != 0
        conn.execute(
            """INSERT INTO dryer_readings
               (exhaust_temp, ambient_temp, diff_pressure, exhaust_humidity,
                vibration_rms, current_ma, dryer_state, heating_on,
                fire_risk_score, lint_clog_level, dryness_level,
                alert_level, battery_pct)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)""",
            (payload.get("exhaust_temp"), payload.get("ambient_temp"),
             payload.get("diff_pressure"), payload.get("exhaust_humidity"),
             payload.get("vibration_rms"), payload.get("current_ma"),
             payload.get("dryer_state"), payload.get("heating_on"),
             payload.get("fire_risk_score"), payload.get("lint_clog_level"),
             payload.get("dryness_level"), payload.get("alert_level"),
             payload.get("battery_pct")),
        )
        # Auto-alert based on dryer alert level
        alert_level = payload.get("alert_level", 0)
        if alert_level >= 3:
            level = "CRITICAL" if alert_level == 3 else "EMERGENCY"
            msg_text = "FIRE RISK: Clean lint trap immediately!"
            if payload.get("exhaust_temp", 0) > 95:
                msg_text = f"FIRE RISK: Exhaust {payload['exhaust_temp']}°C — check dryer!"
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                (level, "dryer", msg_text),
            )

    elif topic == "washwise/scan_result":
        latest_data["scanner"] = payload
        conn.execute(
            """INSERT INTO scan_results
               (fabric_type, fabric_confidence, stain_type, stain_confidence,
                wash_temp, recommended_cycle, detergent_ml, pre_treat_id)
               VALUES (?,?,?,?,?,?,?,?)""",
            (payload.get("fabric_type"), payload.get("fabric_confidence"),
             payload.get("stain_type"), payload.get("stain_confidence"),
             payload.get("wash_temp"), payload.get("recommended_cycle"),
             payload.get("detergent_ml"), payload.get("pre_treat_id")),
        )

    elif topic == "washwise/fire_alert":
        # Critical fire alert — log + push
        conn.execute(
            "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
            ("EMERGENCY", "dryer",
             f"FIRE ALERT: risk={payload.get('fire_risk_score')}/255, "
             f"exhaust={payload.get('exhaust_temp')}°C"),
        )

    elif topic == "washwise/energy_data":
        conn.execute(
            """INSERT INTO energy_log
               (node_type, cycle_energy_wh, cycle_water_ml, cycle_duration_s,
                estimated_cost_cents, co2_g)
               VALUES (?,?,?,?,?,?)""",
            (payload.get("node_type"), payload.get("cycle_energy_wh"),
             payload.get("cycle_water_ml"), payload.get("cycle_duration_s"),
             payload.get("estimated_cost_cents"), payload.get("co2_g")),
        )

    elif topic == "washwise/alerts":
        conn.execute(
            "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
            (payload.get("level", "INFO"), payload.get("source", "system"),
             payload.get("message", "")),
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
ws_clients: List[WebSocket] = []


@app.websocket("/ws/live")
async def websocket_live(websocket: WebSocket):
    await websocket.accept()
    ws_clients.append(websocket)
    try:
        while True:
            await websocket.send_json(latest_data)
            await asyncio.sleep(1)
    except Exception:
        ws_clients.remove(websocket)


# ---- REST API ----

class DoseCommand(BaseModel):
    detergent_ml: int
    reason: str = "manual"


class CycleSelectCommand(BaseModel):
    cycle_type: int  # 0=normal,1=delicate,2=heavy,3=quick,4=handwash
    temp_c: float = 40.0


class AlertAck(BaseModel):
    alert_id: int


@app.get("/api/status")
async def get_status():
    """Overall system status — fire risk is the headline number."""
    return latest_data


@app.get("/api/washer/latest")
async def get_washer_latest():
    return latest_data.get("washer", {})


@app.get("/api/washer/history")
async def get_washer_history(hours: int = 24):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM washer_readings ORDER BY timestamp DESC LIMIT ?",
        (hours * 240,),
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/dryer/latest")
async def get_dryer_latest():
    return latest_data.get("dryer", {})


@app.get("/api/dryer/history")
async def get_dryer_history(hours: int = 24):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM dryer_readings ORDER BY timestamp DESC LIMIT ?",
        (hours * 360,),
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/fire_risk")
async def get_fire_risk():
    """Current fire risk score + recent history."""
    risk = latest_data.get("fire_risk", 0.0)
    level = "OK"
    if risk > 0.95:
        level = "EMERGENCY"
    elif risk > 0.8:
        level = "CRITICAL"
    elif risk > 0.6:
        level = "WARNING"

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    recent = conn.execute(
        "SELECT * FROM dryer_readings ORDER BY timestamp DESC LIMIT 60"
    ).fetchall()
    conn.close()
    return {
        "risk_score": risk,
        "level": level,
        "dryer_active": latest_data.get("dryer_active", False),
        "recent_temps": [r["exhaust_temp"] for r in recent],
    }


@app.get("/api/scans/latest")
async def get_latest_scan():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    row = conn.execute(
        "SELECT * FROM scan_results ORDER BY timestamp DESC LIMIT 1"
    ).fetchone()
    conn.close()
    return dict(row) if row else {}


@app.get("/api/scans/history")
async def get_scan_history(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM scan_results ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/alerts")
async def get_alerts(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM alerts ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.post("/api/alerts/ack")
async def ack_alert(ack: AlertAck):
    conn = sqlite3.connect(DB_PATH)
    conn.execute("UPDATE alerts SET acknowledged=1 WHERE id=?", (ack.alert_id,))
    conn.commit()
    conn.close()
    return {"status": "acknowledged", "id": ack.alert_id}


@app.post("/api/dose")
async def send_dose(cmd: DoseCommand):
    """Send detergent dosing command to washer node via hub."""
    mqtt_client.publish("washwise/commands/dose", json.dumps({
        "detergent_ml": cmd.detergent_ml,
        "reason": cmd.reason,
    }))
    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
        ("INFO", "washer", f"Manual dose: {cmd.detergent_ml} mL ({cmd.reason})"),
    )
    conn.commit()
    conn.close()
    return {"status": "sent", "detergent_ml": cmd.detergent_ml}


@app.post("/api/cycle_select")
async def send_cycle_select(cmd: CycleSelectCommand):
    """Send cycle selection to washer node."""
    mqtt_client.publish("washwise/commands/cycle", json.dumps({
        "cycle_type": cmd.cycle_type,
        "temp_c": cmd.temp_c,
    }))
    return {"status": "sent", "cycle_type": cmd.cycle_type, "temp_c": cmd.temp_c}


@app.get("/api/energy")
async def get_energy(days: int = 30):
    """Energy and water usage summary."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM energy_log ORDER BY timestamp DESC LIMIT ?", (days * 20,)
    ).fetchall()
    conn.close()
    total_energy_wh = sum(r["cycle_energy_wh"] or 0 for r in rows)
    total_water_ml = sum(r["cycle_water_ml"] or 0 for r in rows)
    total_cost = sum(r["estimated_cost_cents"] or 0 for r in rows)
    total_co2 = sum(r["co2_g"] or 0 for r in rows)
    return {
        "cycles": len(rows),
        "total_energy_kwh": total_energy_wh / 1000,
        "total_water_l": total_water_ml / 1000,
        "total_cost_dollars": total_cost / 100,
        "total_co2_kg": total_co2 / 1000,
        "records": [dict(r) for r in rows],
    }


# ---- Stain Treatment Library ----

STAIN_DB = {
    0: {"name": "Clean", "treatment": "None needed", "difficulty": "easy"},
    1: {"name": "Coffee/Tea", "treatment": "Rinse with cold water. Apply enzyme detergent. Wash at 40°C.", "difficulty": "medium"},
    2: {"name": "Red Wine", "treatment": "Blot (don't rub). Cover with salt. Rinse with club soda. Apply white vinegar. Wash cold.", "difficulty": "hard"},
    3: {"name": "Blood", "treatment": "COLD water only (heat sets blood permanently). Hydrogen peroxide on cotton. Wash cold.", "difficulty": "medium"},
    4: {"name": "Grease/Oil", "treatment": "Apply dish soap, work in gently. Let sit 10 min. Wash warm.", "difficulty": "medium"},
    5: {"name": "Grass", "treatment": "Rubbing alcohol or enzyme detergent. Let sit 15 min. Wash warm.", "difficulty": "medium"},
    6: {"name": "Ink", "treatment": "Hairspray or rubbing alcohol on cotton ball. Blot from back of fabric. Wash cold.", "difficulty": "hard"},
    7: {"name": "Food", "treatment": "Scrape off solids. Rinse cold. Enzyme detergent. Wash warm.", "difficulty": "easy"},
    8: {"name": "Sweat", "treatment": "Soak in white vinegar (1:1 with water) 30 min. Enzyme detergent. Wash warm.", "difficulty": "easy"},
    9: {"name": "Rust", "treatment": "Lemon juice + salt, sit in sun. Or commercial rust remover. Wash cold.", "difficulty": "hard"},
    10: {"name": "Unknown", "treatment": "Treat as grease: dish soap. If that fails, try enzyme detergent.", "difficulty": "medium"},
}

# ---- Fabric Care Database ----

FABRIC_DB = {
    0: {"name": "Unknown", "max_temp": 40, "cycle": "normal", "bleach": "ok", "tumble_dry": True},
    1: {"name": "Cotton", "max_temp": 60, "cycle": "normal", "bleach": "ok", "tumble_dry": True},
    2: {"name": "Polyester", "max_temp": 40, "cycle": "delicate", "bleach": "no", "tumble_dry": True},
    3: {"name": "Wool", "max_temp": 30, "cycle": "handwash", "bleach": "no", "tumble_dry": False},
    4: {"name": "Silk", "max_temp": 30, "cycle": "handwash", "bleach": "no", "tumble_dry": False},
    5: {"name": "Denim", "max_temp": 40, "cycle": "normal", "bleach": "no", "tumble_dry": True},
    6: {"name": "Nylon", "max_temp": 30, "cycle": "delicate", "bleach": "no", "tumble_dry": True},
    7: {"name": "Linen", "max_temp": 40, "cycle": "delicate", "bleach": "ok", "tumble_dry": True},
    8: {"name": "Blend", "max_temp": 40, "cycle": "normal", "bleach": "no", "tumble_dry": True},
}


@app.get("/api/stains")
async def get_stain_db():
    return STAIN_DB


@app.get("/api/stains/{stain_id}")
async def get_stain_detail(stain_id: int):
    if stain_id in STAIN_DB:
        return STAIN_DB[stain_id]
    raise HTTPException(status_code=404, detail="Stain type not found")


@app.get("/api/fabrics")
async def get_fabric_db():
    return FABRIC_DB


@app.get("/api/fabrics/{fabric_id}")
async def get_fabric_detail(fabric_id: int):
    if fabric_id in FABRIC_DB:
        return FABRIC_DB[fabric_id]
    raise HTTPException(status_code=404, detail="Fabric type not found")


@app.get("/api/maintenance")
async def get_maintenance_log():
    """Lint cleaning + maintenance history."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        """SELECT * FROM alerts WHERE source='dryer' AND message LIKE '%lint%'
           ORDER BY timestamp DESC LIMIT 50"""
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
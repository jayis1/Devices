"""
PorchGuard — FastAPI Backend
Real-time porch security dashboard + MQTT bridge + pirate/tamper alerting +
courier code management + clip index + delivery log + anomaly detection
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

app = FastAPI(title="PorchGuard Dashboard API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
DB_PATH = os.environ.get("DB_PATH", "/data/porchguard.db")


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS camera_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            presence_state INTEGER, person_id INTEGER, person_conf REAL,
            parcel_class INTEGER, parcel_conf REAL, pirate_risk REAL,
            mmwave_dist_cm REAL, ambient_temp REAL, armed INTEGER,
            tamper_flag INTEGER, knock_detected INTEGER, clip_id INTEGER,
            battery_pct INTEGER, wifi_up INTEGER, power_lost INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS mailbox_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            door_state INTEGER, mail_class INTEGER, weight_mg REAL,
            temp REAL, light_lux REAL, tamper_flag INTEGER,
            battery_pct INTEGER, solar_mv REAL, last_event INTEGER,
            event_age_s REAL
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS lock_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            lock_state INTEGER, door_state INTEGER, last_unlock_src INTEGER,
            last_code_id INTEGER, tamper_flag INTEGER, battery_pct INTEGER,
            auto_lock_enabled INTEGER, door_open_s REAL,
            garage_relay_on INTEGER, codes_active INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS deliveries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            event_type INTEGER, parcel_class INTEGER, courier_id INTEGER,
            source_node INTEGER, has_clip INTEGER, clip_id INTEGER,
            ambient_temp REAL, weight_mg REAL, outcome TEXT DEFAULT 'delivered'
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS visitors (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            person_id INTEGER, person_conf REAL, duration_s REAL,
            had_clip INTEGER, clip_id INTEGER, label TEXT
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            level TEXT, source TEXT, message TEXT, clip_id INTEGER,
            acknowledged BOOLEAN DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS courier_codes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            code_id INTEGER UNIQUE, code_digits TEXT, issued_at DATETIME,
            expires_at DATETIME, used INTEGER DEFAULT 0, revoked INTEGER DEFAULT 0,
            delivery_id INTEGER, note TEXT
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS lock_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            action TEXT, source INTEGER, code_id INTEGER
        )
    """)
    conn.commit()
    conn.close()


init_db()

# ---- MQTT Bridge ----
MQTT_BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

latest_data = {
    "camera": {},
    "mailbox": {},
    "lock": {},
    "pirate_risk": 0.0,
    "porch_active": False,
    "armed": True,
    "siren_active": False,
}

mqtt_client = mqtt.Client(client_id="porchguard-dashboard")


def on_connect(client, userdata, flags, rc):
    print(f"MQTT connected (rc={rc})")
    client.subscribe("porchguard/camera_data")
    client.subscribe("porchguard/mailbox_data")
    client.subscribe("porchguard/lock_data")
    client.subscribe("porchguard/pirate_alert")
    client.subscribe("porchguard/tamper_alert")
    client.subscribe("porchguard/delivery_event")
    client.subscribe("porchguard/clip_ref")
    client.subscribe("porchguard/alerts")
    client.subscribe("porchguard/lock_event")


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        return

    conn = sqlite3.connect(DB_PATH)

    if topic == "porchguard/camera_data":
        latest_data["camera"] = payload
        latest_data["pirate_risk"] = payload.get("pirate_risk", 0) / 255.0
        latest_data["porch_active"] = payload.get("presence_state", 0) != 0
        conn.execute(
            """INSERT INTO camera_readings
               (presence_state, person_id, person_conf, parcel_class, parcel_conf,
                pirate_risk, mmwave_dist_cm, ambient_temp, armed, tamper_flag,
                knock_detected, clip_id, battery_pct, wifi_up, power_lost)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)""",
            (payload.get("presence_state"), payload.get("person_id"),
             payload.get("person_conf"), payload.get("parcel_class"),
             payload.get("parcel_conf"), payload.get("pirate_risk"),
             payload.get("mmwave_dist_cm"), payload.get("ambient_temp"),
             payload.get("armed"), payload.get("tamper_flag"),
             payload.get("knock_detected"), payload.get("clip_id"),
             payload.get("battery_pct"), payload.get("wifi_up"),
             payload.get("power_lost")),
        )
        # Stranger loitering alert
        if payload.get("person_id", 0) == 4:  # PERSON_LOITERING
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                ("WARNING", "camera", "Unknown person loitering on porch"),
            )
        # Camera power lost = possible tamper
        if payload.get("power_lost", 0):
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                ("CRITICAL", "camera", "Camera power lost — possible tamper"),
            )

    elif topic == "porchguard/mailbox_data":
        latest_data["mailbox"] = payload
        conn.execute(
            """INSERT INTO mailbox_readings
               (door_state, mail_class, weight_mg, temp, light_lux,
                tamper_flag, battery_pct, solar_mv, last_event, event_age_s)
               VALUES (?,?,?,?,?,?,?,?,?,?)""",
            (payload.get("door_state"), payload.get("mail_class"),
             payload.get("weight_mg"), payload.get("temp"),
             payload.get("light_lux"), payload.get("tamper_flag"),
             payload.get("battery_pct"), payload.get("solar_mv"),
             payload.get("last_event"), payload.get("event_age_s")),
        )
        # Mail arrived
        if payload.get("last_event") == 1:
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                ("INFO", "mailbox",
                 f"Mail arrived: {['empty','letter','thick','parcel'][payload.get('mail_class', 0)]} "
                 f"({payload.get('weight_mg', 0)}mg)"),
            )

    elif topic == "porchguard/lock_data":
        latest_data["lock"] = payload
        conn.execute(
            """INSERT INTO lock_readings
               (lock_state, door_state, last_unlock_src, last_code_id,
                tamper_flag, battery_pct, auto_lock_enabled, door_open_s,
                garage_relay_on, codes_active)
               VALUES (?,?,?,?,?,?,?,?,?,?)""",
            (payload.get("lock_state"), payload.get("door_state"),
             payload.get("last_unlock_src"), payload.get("last_code_id"),
             payload.get("tamper_flag"), payload.get("battery_pct"),
             payload.get("auto_lock_enabled"), payload.get("door_open_s"),
             payload.get("garage_relay_on"), payload.get("codes_active")),
        )
        # Door left open
        if payload.get("door_open_s", 0) > 120 and payload.get("lock_state") == 1:
            conn.execute(
                "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
                ("WARNING", "lock", "Door left open >2 minutes"),
            )

    elif topic == "porchguard/pirate_alert":
        latest_data["siren_active"] = True
        conn.execute(
            "INSERT INTO alerts (level, source, message, clip_id) VALUES (?,?,?,?)",
            ("EMERGENCY" if payload.get("alert_level", 0) >= 4 else "CRITICAL",
             "camera",
             f"PORCH PIRATE: risk={payload.get('pirate_risk')}/255, "
             f"parcel={payload.get('parcel_class')}",
             payload.get("clip_id")),
        )

    elif topic == "porchguard/tamper_alert":
        tamper_names = {0: "cover-moved", 1: "tilt", 2: "forced-entry",
                        3: "power-cut", 4: "fishing"}
        source_names = {1: "camera", 2: "mailbox", 3: "lock"}
        conn.execute(
            "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
            ("EMERGENCY" if payload.get("severity", 0) >= 3 else "CRITICAL",
             source_names.get(payload.get("source_node", 0), "system"),
             f"TAMPER: {tamper_names.get(payload.get('tamper_type', 0), 'unknown')}"),
        )

    elif topic == "porchguard/delivery_event":
        conn.execute(
            """INSERT INTO deliveries
               (event_type, parcel_class, courier_id, source_node, has_clip,
                clip_id, ambient_temp, weight_mg)
               VALUES (?,?,?,?,?,?,?,?)""",
            (payload.get("event_type"), payload.get("parcel_class"),
             payload.get("courier_id"), payload.get("source_node"),
             payload.get("has_clip"), payload.get("clip_id"),
             payload.get("temp_c_x10"), payload.get("weight_mg_hi", 0)),
        )
        courier_names = {0: "Unknown", 1: "UPS", 2: "FedEx", 3: "USPS",
                         4: "Amazon", 5: "DHL", 6: "Other"}
        conn.execute(
            "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
            ("INFO", "porch",
             f"Package delivered by {courier_names.get(payload.get('courier_id', 0), 'Unknown')}"),
        )

    elif topic == "porchguard/lock_event":
        conn.execute(
            "INSERT INTO lock_events (action, source, code_id) VALUES (?,?,?)",
            (payload.get("action"), payload.get("source"), payload.get("code_id")),
        )

    elif topic == "porchguard/alerts":
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

class UnlockCommand(BaseModel):
    source: int = 0  # 0=app, 1=keypad, 2=auto, 3=courier
    code_id: int = 0


class IssueCodeCommand(BaseModel):
    window_minutes: int = 60
    delivery_note: str = ""


class ArmCommand(BaseModel):
    armed: bool = True


class SirenCommand(BaseModel):
    duration_s: int = 15


class GarageCommand(BaseModel):
    duration_s: int = 1


class AlertAck(BaseModel):
    alert_id: int


@app.get("/api/status")
async def get_status():
    """Overall system status — pirate risk is the headline number."""
    return latest_data


@app.get("/api/camera/latest")
async def get_camera_latest():
    return latest_data.get("camera", {})


@app.get("/api/camera/history")
async def get_camera_history(hours: int = 24):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM camera_readings ORDER BY timestamp DESC LIMIT ?",
        (hours * 240,),
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/mailbox/latest")
async def get_mailbox_latest():
    return latest_data.get("mailbox", {})


@app.get("/api/lock/latest")
async def get_lock_latest():
    return latest_data.get("lock", {})


@app.get("/api/lock/history")
async def get_lock_history(limit: int = 100):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM lock_readings ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/deliveries")
async def get_deliveries(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM deliveries ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/visitors")
async def get_visitors(limit: int = 50):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM visitors ORDER BY timestamp DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/pirate_risk")
async def get_pirate_risk():
    """Current pirate risk score + recent history."""
    risk = latest_data.get("pirate_risk", 0.0)
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
        "SELECT * FROM camera_readings ORDER BY timestamp DESC LIMIT 60"
    ).fetchall()
    conn.close()
    return {
        "risk_score": risk,
        "level": level,
        "porch_active": latest_data.get("porch_active", False),
        "recent_risks": [r["pirate_risk"] for r in recent],
    }


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


# ---- Lock control ----

@app.post("/api/unlock")
async def send_unlock(cmd: UnlockCommand):
    """Send unlock command to lock node via hub (BLE or Sub-GHz)."""
    mqtt_client.publish("porchguard/commands/unlock", json.dumps({
        "source": cmd.source, "code_id": cmd.code_id,
    }))
    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        "INSERT INTO lock_events (action, source, code_id) VALUES (?,?,?)",
        ("UNLOCK", cmd.source, cmd.code_id),
    )
    conn.execute(
        "INSERT INTO alerts (level, source, message) VALUES (?,?,?)",
        ("INFO", "lock", f"Unlock requested (source={cmd.source})"),
    )
    conn.commit()
    conn.close()
    return {"status": "sent", "source": cmd.source}


@app.post("/api/lock")
async def send_lock():
    mqtt_client.publish("porchguard/commands/lock", json.dumps({}))
    return {"status": "sent", "action": "lock"}


@app.post("/api/garage")
async def send_garage(cmd: GarageCommand):
    mqtt_client.publish("porchguard/commands/garage", json.dumps({
        "duration_s": cmd.duration_s,
    }))
    return {"status": "sent", "duration_s": cmd.duration_s}


# ---- One-time courier codes ----

import random


@app.post("/api/codes/issue")
async def issue_courier_code(cmd: IssueCodeCommand):
    """Issue a one-time 6-digit courier code valid for a time window."""
    code_digits = f"{random.randint(0, 999999):06d}"
    issued_at = datetime.utcnow()
    expires_at = datetime.utcnow().timestamp() + cmd.window_minutes * 60

    conn = sqlite3.connect(DB_PATH)
    cur = conn.execute(
        """INSERT INTO courier_codes
           (code_digits, issued_at, expires_at, note)
           VALUES (?,?,?,?)""",
        (code_digits, issued_at, expires_at, cmd.delivery_note),
    )
    code_id = cur.lastrowid
    conn.commit()
    conn.close()

    # Send to lock node (stored in flash)
    mqtt_client.publish("porchguard/commands/issue_code", json.dumps({
        "code_id": code_id,
        "code_digits": code_digits,
        "window_minutes": cmd.window_minutes,
    }))

    return {
        "status": "issued",
        "code_id": code_id,
        "code": code_digits,
        "valid_minutes": cmd.window_minutes,
        "note": cmd.delivery_note,
    }


@app.get("/api/codes/active")
async def get_active_codes():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    now = datetime.utcnow().timestamp()
    rows = conn.execute(
        "SELECT * FROM courier_codes WHERE used=0 AND revoked=0 ORDER BY issued_at DESC"
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.post("/api/codes/revoke/{code_id}")
async def revoke_code(code_id: int):
    conn = sqlite3.connect(DB_PATH)
    conn.execute("UPDATE courier_codes SET revoked=1 WHERE id=?", (code_id,))
    conn.commit()
    conn.close()
    mqtt_client.publish("porchguard/commands/revoke_code", json.dumps({
        "code_id": code_id,
    }))
    return {"status": "revoked", "code_id": code_id}


# ---- Arm/disarm ----

@app.post("/api/arm")
async def set_arm(cmd: ArmCommand):
    mqtt_client.publish("porchguard/commands/arm", json.dumps({"armed": cmd.armed}))
    latest_data["armed"] = cmd.armed
    return {"status": "sent", "armed": cmd.armed}


# ---- Siren ----

@app.post("/api/siren")
async def trigger_siren(cmd: SirenCommand):
    mqtt_client.publish("porchguard/commands/siren", json.dumps({
        "duration_s": cmd.duration_s,
    }))
    return {"status": "sent", "duration_s": cmd.duration_s}


@app.post("/api/siren/off")
async def stop_siren():
    mqtt_client.publish("porchguard/commands/siren_off", json.dumps({}))
    latest_data["siren_active"] = False
    return {"status": "sent"}


# ---- Delivery statistics ----

@app.get("/api/delivery_stats")
async def get_delivery_stats(days: int = 30):
    """Delivery summary — packages, couriers, theft rate."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT * FROM deliveries ORDER BY timestamp DESC LIMIT ?", (days * 50,)
    ).fetchall()
    conn.close()
    courier_counts = {}
    for r in rows:
        cid = r["courier_id"] or 0
        courier_counts[cid] = courier_counts.get(cid, 0) + 1
    stolen = sum(1 for r in rows if r["outcome"] == "stolen")
    return {
        "total_deliveries": len(rows),
        "by_courier": courier_counts,
        "stolen": stolen,
        "theft_rate": stolen / max(len(rows), 1),
    }


# ---- Courier / person reference ----

COURIER_DB = {
    0: {"name": "Unknown"},
    1: {"name": "UPS", "color": "#8B4513"},
    2: {"name": "FedEx", "color": "#4A148C"},
    3: {"name": "USPS", "color": "#1565C0"},
    4: {"name": "Amazon", "color": "#FF9900"},
    5: {"name": "DHL", "color": "#D32F2F"},
    6: {"name": "Other", "color": "#607D8B"},
}

PERSON_DB = {
    0: {"name": "None"},
    1: {"name": "Resident", "color": "#4CAF50"},
    2: {"name": "Courier", "color": "#2196F3"},
    3: {"name": "Unknown", "color": "#FF9800"},
    4: {"name": "Loitering", "color": "#F44336"},
}


@app.get("/api/couriers")
async def get_courier_db():
    return COURIER_DB


@app.get("/api/persons")
async def get_person_db():
    return PERSON_DB


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
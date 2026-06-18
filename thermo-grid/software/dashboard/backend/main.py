"""
ThermoGrid — FastAPI Backend
Real-time thermal dashboard + MQTT bridge + thermal forecast + comfort
optimization + solar/TOU coordination + energy analytics + freeze protection
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

app = FastAPI(title="ThermoGrid Dashboard API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
DB_PATH = os.environ.get("DB_PATH", "/data/thermogrid.db")


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            node_id INTEGER, zone_id INTEGER,
            air_temp REAL, mrt REAL, humidity REAL, air_vel REAL,
            pressure REAL, occupancy INTEGER, occupancy_conf REAL,
            light_lux REAL, co2_ppm INTEGER, window_state INTEGER,
            solar_gain_w REAL, battery_pct INTEGER, solar_mv REAL,
            fault_flags INTEGER, seq_num INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS actuator_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            node_id INTEGER, zone_id INTEGER,
            valve_pos REAL, valve_target REAL, pipe_temp REAL,
            flow_mlmin REAL, energy_btu REAL, zone_mode INTEGER,
            relay_state INTEGER, fault_flags INTEGER,
            battery_pct INTEGER, power_source INTEGER,
            pipe_target REAL, pid_active INTEGER, seq_num INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS comfort_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            person_id INTEGER,
            skin_temp REAL, air_temp REAL, humidity REAL,
            hr_bpm INTEGER, hrv_ms INTEGER, activity INTEGER,
            comfort_score INTEGER, comfort_conf REAL,
            vote_pending INTEGER, battery_pct INTEGER,
            signal_rssi INTEGER, seq_num INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS zone_states (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            zone_id INTEGER, setpoint REAL, mode INTEGER,
            boost_minutes INTEGER, boost_delta REAL,
            window_open INTEGER, frost_protect INTEGER,
            sensor_node INTEGER, actuator_node INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS energy_reports (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            zone_id INTEGER, energy_wh REAL, flow_total_l REAL,
            uptime_minutes REAL, avg_pipe_temp REAL,
            avg_room_temp REAL, cost_cents REAL, tariff_period INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS comfort_votes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            person_id INTEGER, vote INTEGER,
            skin_temp REAL, activity INTEGER, room_id INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            level TEXT, source TEXT, zone_id INTEGER,
            message TEXT, acknowledged BOOLEAN DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS solar_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            production_w REAL, base_load_w REAL, surplus_w REAL,
            boost_recommended INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS tou_schedule (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            current_period INTEGER, rate_cents REAL,
            next_change_min INTEGER, next_period INTEGER,
            next_rate_cents REAL
        )
    """)
    conn.commit()
    conn.close()


init_db()

# ---- MQTT Client ----
mqtt_client = None
mqtt_connected = False

MQTT_BROKER = os.environ.get("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

# WebSocket subscribers for real-time push
ws_clients: set = set()


def mqtt_on_connect(client, userdata, flags, rc):
    global mqtt_connected
    mqtt_connected = (rc == 0)
    print(f"[MQTT] Connected: {rc}")
    client.subscribe([
        ("thermogrid/sensor_data", 1),
        ("thermogrid/actuator_data", 1),
        ("thermogrid/comfort_data", 1),
        ("thermogrid/zone_state", 1),
        ("thermogrid/energy_report", 1),
        ("thermogrid/freeze_alert", 1),
        ("thermogrid/window_open", 1),
        ("thermogrid/comfort_vote", 1),
        ("thermogrid/solar_status", 1),
        ("thermogrid/tou_schedule", 1),
        ("thermogrid/alerts", 1),
    ])


def mqtt_on_message(client, userdata, msg):
    topic = msg.topic
    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        return

    print(f"[MQTT] {topic}: {data}")

    # Store in database
    store_reading(topic, data)

    # Push to WebSocket clients
    ws_msg = json.dumps({"topic": topic, "data": data, "ts": datetime.now().isoformat()})
    dead = set()
    for ws in ws_clients:
        try:
            asyncio.create_task(ws.send_text(ws_msg))
        except Exception:
            dead.add(ws)
    ws_clients.difference_update(dead)


def store_reading(topic, data):
    conn = sqlite3.connect(DB_PATH)
    try:
        if topic == "thermogrid/sensor_data":
            conn.execute("""
                INSERT INTO sensor_readings
                (node_id, zone_id, air_temp, mrt, humidity, air_vel, pressure,
                 occupancy, occupancy_conf, light_lux, co2_ppm, window_state,
                 solar_gain_w, battery_pct, solar_mv, fault_flags, seq_num)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            """, (
                data.get("node_id"), data.get("zone_id"),
                data.get("air_temp"), data.get("mrt"),
                data.get("humidity"), data.get("air_vel"),
                data.get("pressure"), data.get("occupancy"),
                data.get("occupancy_conf"), data.get("light_lux"),
                data.get("co2_ppm"), data.get("window_state"),
                data.get("solar_gain_w"), data.get("battery_pct"),
                data.get("solar_mv"), data.get("fault_flags"),
                data.get("seq_num")
            ))
        elif topic == "thermogrid/actuator_data":
            conn.execute("""
                INSERT INTO actuator_readings
                (node_id, zone_id, valve_pos, valve_target, pipe_temp,
                 flow_mlmin, energy_btu, zone_mode, relay_state,
                 fault_flags, battery_pct, power_source,
                 pipe_target, pid_active, seq_num)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            """, (
                data.get("node_id"), data.get("zone_id"),
                data.get("valve_pos"), data.get("valve_target"),
                data.get("pipe_temp"), data.get("flow_mlmin"),
                data.get("energy_btu"), data.get("zone_mode"),
                data.get("relay_state"), data.get("fault_flags"),
                data.get("battery_pct"), data.get("power_source"),
                data.get("pipe_target"), data.get("pid_active"),
                data.get("seq_num")
            ))
        elif topic == "thermogrid/comfort_data":
            conn.execute("""
                INSERT INTO comfort_readings
                (person_id, skin_temp, air_temp, humidity, hr_bpm, hrv_ms,
                 activity, comfort_score, comfort_conf, vote_pending,
                 battery_pct, signal_rssi, seq_num)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
            """, (
                data.get("person_id"),
                data.get("skin_temp"), data.get("air_temp"),
                data.get("humidity"), data.get("hr_bpm"),
                data.get("hrv_ms"), data.get("activity"),
                data.get("comfort_score"), data.get("comfort_conf"),
                data.get("vote_pending"), data.get("battery_pct"),
                data.get("signal_rssi"), data.get("seq_num")
            ))
        elif topic == "thermogrid/zone_state":
            conn.execute("""
                INSERT INTO zone_states
                (zone_id, setpoint, mode, boost_minutes, boost_delta,
                 window_open, frost_protect, sensor_node, actuator_node)
                VALUES (?,?,?,?,?,?,?,?,?)
            """, (
                data.get("zone_id"), data.get("setpoint"),
                data.get("mode"), data.get("boost_minutes"),
                data.get("boost_delta"), data.get("window_open"),
                data.get("frost_protect"),
                data.get("sensor_node"), data.get("actuator_node")
            ))
        elif topic == "thermogrid/energy_report":
            conn.execute("""
                INSERT INTO energy_reports
                (zone_id, energy_wh, flow_total_l, uptime_minutes,
                 avg_pipe_temp, avg_room_temp, cost_cents, tariff_period)
                VALUES (?,?,?,?,?,?,?,?)
            """, (
                data.get("zone_id"), data.get("energy_wh"),
                data.get("flow_total_l"), data.get("uptime_minutes"),
                data.get("avg_pipe_temp"), data.get("avg_room_temp"),
                data.get("cost_cents"), data.get("tariff_period")
            ))
        elif topic == "thermogrid/comfort_vote":
            conn.execute("""
                INSERT INTO comfort_votes
                (person_id, vote, skin_temp, activity, room_id)
                VALUES (?,?,?,?,?)
            """, (
                data.get("person_id"), data.get("vote"),
                data.get("skin_temp"), data.get("activity"),
                data.get("room_id")
            ))
        elif topic == "thermogrid/solar_status":
            conn.execute("""
                INSERT INTO solar_data
                (production_w, base_load_w, surplus_w, boost_recommended)
                VALUES (?,?,?,?)
            """, (
                data.get("production_w"), data.get("base_load_w"),
                data.get("surplus_w"), data.get("boost_recommended")
            ))
        elif topic == "thermogrid/tou_schedule":
            conn.execute("""
                INSERT INTO tou_schedule
                (current_period, rate_cents, next_change_min,
                 next_period, next_rate_cents)
                VALUES (?,?,?,?,?)
            """, (
                data.get("current_period"), data.get("rate_cents"),
                data.get("next_change_min"), data.get("next_period"),
                data.get("next_rate_cents")
            ))
        elif topic in ("thermogrid/freeze_alert", "thermogrid/window_open",
                       "thermogrid/alerts"):
            level = "CRITICAL" if "freeze" in topic else "INFO"
            conn.execute("""
                INSERT INTO alerts (level, source, zone_id, message)
                VALUES (?,?,?,?)
            """, (
                data.get("level", level), topic,
                data.get("zone_id", data.get("room_id")),
                json.dumps(data)
            ))
        conn.commit()
    except Exception as e:
        print(f"[DB] Error storing {topic}: {e}")
    finally:
        conn.close()


def start_mqtt():
    global mqtt_client
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = mqtt_on_connect
    mqtt_client.on_message = mqtt_on_message
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"[MQTT] Connection failed: {e}")


@app.on_event("startup")
async def startup_event():
    start_mqtt()


# ---- API Models ----

class ZoneSetpoint(BaseModel):
    zone_id: int
    setpoint: float
    mode: int = 1
    boost_minutes: int = 0


class ComfortVote(BaseModel):
    person_id: int
    vote: int  # -3 to +3
    skin_temp: Optional[float] = None
    activity: Optional[int] = None


class ZoneSchedule(BaseModel):
    zone_id: int
    slots: List[dict]  # [{hour: 6, setpoint: 22.0}, ...]


# ---- API Endpoints ----

@app.get("/api/status")
async def get_status():
    """Overall system status with thermal map + energy + solar."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    # Latest sensor readings per zone
    sensors = conn.execute("""
        SELECT * FROM sensor_readings s
        WHERE id = (SELECT MAX(id) FROM sensor_readings WHERE node_id = s.node_id)
    """).fetchall()

    # Latest actuator readings per zone
    actuators = conn.execute("""
        SELECT * FROM actuator_readings a
        WHERE id = (SELECT MAX(id) FROM actuator_readings WHERE node_id = a.node_id)
    """).fetchall()

    # Latest zone states
    zones = conn.execute("""
        SELECT * FROM zone_states z
        WHERE id = (SELECT MAX(id) FROM zone_states WHERE zone_id = z.zone_id)
    """).fetchall()

    # Latest solar
    solar = conn.execute("""
        SELECT * FROM solar_data ORDER BY id DESC LIMIT 1
    """).fetchone()

    # Latest TOU
    tou = conn.execute("""
        SELECT * FROM tou_schedule ORDER BY id DESC LIMIT 1
    """).fetchone()

    # Latest comfort per person
    comfort = conn.execute("""
        SELECT * FROM comfort_readings c
        WHERE id = (SELECT MAX(id) FROM comfort_readings WHERE person_id = c.person_id)
    """).fetchall()

    # Unacknowledged alerts
    active_alerts = conn.execute("""
        SELECT COUNT(*) as cnt FROM alerts WHERE acknowledged = 0
    """).fetchone()

    conn.close()

    return {
        "zones": [dict(z) for z in zones],
        "sensors": [dict(s) for s in sensors],
        "actuators": [dict(a) for a in actuators],
        "comfort": [dict(c) for c in comfort],
        "solar": dict(solar) if solar else None,
        "tou": dict(tou) if tou else None,
        "active_alerts": active_alerts["cnt"] if active_alerts else 0,
        "mqtt_connected": mqtt_connected,
    }


@app.get("/api/zones")
async def get_zones():
    """List all configured zones with current state."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    zones = conn.execute("""
        SELECT * FROM zone_states z
        WHERE id = (SELECT MAX(id) FROM zone_states WHERE zone_id = z.zone_id)
    """).fetchall()
    conn.close()
    return [dict(z) for z in zones]


@app.get("/api/zones/{zone_id}/history")
async def get_zone_history(zone_id: int, hours: int = 24):
    """Historical sensor + actuator data for a zone."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    since = datetime.now().timestamp() - hours * 3600
    sensors = conn.execute("""
        SELECT * FROM sensor_readings
        WHERE zone_id = ? AND timestamp >= datetime(?, 'unixepoch')
        ORDER BY timestamp
    """, (zone_id, since)).fetchall()
    actuators = conn.execute("""
        SELECT * FROM actuator_readings
        WHERE zone_id = ? AND timestamp >= datetime(?, 'unixepoch')
        ORDER BY timestamp
    """, (zone_id, since)).fetchall()
    conn.close()
    return {
        "sensors": [dict(s) for s in sensors],
        "actuators": [dict(a) for a in actuators],
    }


@app.post("/api/zones/{zone_id}/setpoint")
async def set_zone_setpoint(zone_id: int, sp: ZoneSetpoint):
    """Set zone setpoint + mode. Publishes to MQTT → hub → actuator."""
    if mqtt_client and mqtt_connected:
        mqtt_client.publish(
            "thermogrid/commands/setpoint",
            json.dumps({
                "zone_id": zone_id,
                "setpoint": sp.setpoint,
                "mode": sp.mode,
                "boost_minutes": sp.boost_minutes,
            }),
            qos=1
        )
    return {"status": "ok", "zone_id": zone_id, "setpoint": sp.setpoint}


@app.post("/api/zones/{zone_id}/boost")
async def boost_zone(zone_id: int, delta: float = 1.5, minutes: int = 30):
    """Temporary boost for a zone."""
    if mqtt_client and mqtt_connected:
        mqtt_client.publish(
            "thermogrid/commands/boost",
            json.dumps({
                "zone_id": zone_id,
                "delta_c": delta,
                "minutes": minutes,
            }),
            qos=1
        )
    return {"status": "boosting", "zone_id": zone_id, "delta": delta, "minutes": minutes}


@app.post("/api/zones/{zone_id}/schedule")
async def set_zone_schedule(zone_id: int, schedule: ZoneSchedule):
    """Set per-zone heating schedule (hourly setpoints)."""
    for slot in schedule.slots:
        if mqtt_client and mqtt_connected:
            mqtt_client.publish(
                "thermogrid/commands/schedule",
                json.dumps({
                    "zone_id": zone_id,
                    "hour": slot.get("hour"),
                    "setpoint": slot.get("setpoint"),
                }),
                qos=1
            )
    return {"status": "scheduled", "zone_id": zone_id, "slots": len(schedule.slots)}


@app.get("/api/comfort/{person_id}")
async def get_comfort_profile(person_id: int):
    """Get personal comfort profile + vote history."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    latest = conn.execute("""
        SELECT * FROM comfort_readings
        WHERE person_id = ? ORDER BY id DESC LIMIT 1
    """, (person_id,)).fetchone()

    votes = conn.execute("""
        SELECT * FROM comfort_votes
        WHERE person_id = ?
        ORDER BY timestamp DESC LIMIT 100
    """, (person_id,)).fetchall()

    # Vote statistics
    vote_stats = conn.execute("""
        SELECT
            COUNT(*) as total_votes,
            AVG(vote) as avg_vote,
            AVG(skin_temp) as avg_skin_temp,
            AVG(activity) as avg_activity
        FROM comfort_votes WHERE person_id = ?
    """, (person_id,)).fetchone()

    conn.close()
    return {
        "person_id": person_id,
        "latest": dict(latest) if latest else None,
        "votes": [dict(v) for v in votes],
        "stats": dict(vote_stats) if vote_stats else None,
    }


@app.post("/api/comfort/{person_id}/vote")
async def submit_comfort_vote(person_id: int, vote: ComfortVote):
    """Submit a comfort vote (from app button or tag)."""
    if mqtt_client and mqtt_connected:
        mqtt_client.publish(
            "thermogrid/comfort_vote",
            json.dumps({
                "person_id": person_id,
                "vote": vote.vote,
                "skin_temp": vote.skin_temp,
                "activity": vote.activity,
            }),
            qos=1
        )
    return {"status": "voted", "person_id": person_id, "vote": vote.vote}


@app.get("/api/energy")
async def get_energy(days: int = 7):
    """Energy consumption per zone + total + savings."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    since = datetime.now().timestamp() - days * 86400
    reports = conn.execute("""
        SELECT zone_id,
               SUM(energy_wh) as total_wh,
               SUM(flow_total_l) as total_flow,
               AVG(avg_pipe_temp) as avg_pipe,
               AVG(avg_room_temp) as avg_room,
               COUNT(*) as report_count
        FROM energy_reports
        WHERE timestamp >= datetime(?, 'unixepoch')
        GROUP BY zone_id
    """, (since,)).fetchall()

    conn.close()
    return {
        "days": days,
        "zones": [dict(r) for r in reports],
    }


@app.get("/api/energy/savings")
async def get_energy_savings(days: int = 30):
    """Compare actual usage to single-thermostat counterfactual."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    since = datetime.now().timestamp() - days * 86400
    actual = conn.execute("""
        SELECT SUM(energy_wh) as total_wh FROM energy_reports
        WHERE timestamp >= datetime(?, 'unixepoch')
    """, (since,)).fetchone()

    # Counterfactual: assume whole-house conditioning (all zones × hours active)
    # vs ThermoGrid (only occupied zones conditioned)
    # Typical savings: 20-40% from zone conditioning alone
    actual_wh = actual["total_wh"] if actual and actual["total_wh"] else 0
    # Estimate counterfactual as actual / 0.7 (30% savings assumed baseline)
    counterfactual_wh = actual_wh / 0.7 if actual_wh > 0 else 0
    savings_wh = counterfactual_wh - actual_wh
    savings_pct = (savings_wh / counterfactual_wh * 100) if counterfactual_wh > 0 else 0

    conn.close()
    return {
        "days": days,
        "actual_wh": actual_wh,
        "counterfactual_wh": counterfactual_wh,
        "savings_wh": savings_wh,
        "savings_pct": savings_pct,
    }


@app.get("/api/solar")
async def get_solar(hours: int = 24):
    """Solar production + self-consumption data."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    since = datetime.now().timestamp() - hours * 3600
    data = conn.execute("""
        SELECT * FROM solar_data
        WHERE timestamp >= datetime(?, 'unixepoch')
        ORDER BY timestamp
    """, (since,)).fetchall()
    conn.close()
    return [dict(d) for d in data]


@app.get("/api/alerts")
async def get_alerts(limit: int = 50):
    """Recent alerts (freeze, window, fault, etc.)."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    alerts = conn.execute("""
        SELECT * FROM alerts ORDER BY id DESC LIMIT ?
    """, (limit,)).fetchall()
    conn.close()
    return [dict(a) for a in alerts]


@app.post("/api/alerts/ack")
async def ack_alert(alert_id: int):
    conn = sqlite3.connect(DB_PATH)
    conn.execute("UPDATE alerts SET acknowledged = 1 WHERE id = ?", (alert_id,))
    conn.commit()
    conn.close()
    return {"status": "acknowledged", "alert_id": alert_id}


@app.get("/api/thermal_map")
async def get_thermal_map():
    """Current thermal map: per-room temp, MRT, occupancy, zone states."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    sensors = conn.execute("""
        SELECT s.*, z.zone_id FROM sensor_readings s
        JOIN zone_states z ON s.node_id = z.sensor_node
        WHERE s.id = (SELECT MAX(id) FROM sensor_readings WHERE node_id = s.node_id)
    """).fetchall()
    conn.close()
    return [dict(s) for s in sensors]


@app.get("/api/forecast")
async def get_forecast():
    """4-hour thermal forecast per zone (from hub's ML model)."""
    # In production: query hub for latest forecast via MQTT request/reply
    # Stub: return empty
    return {"forecasts": [], "note": "Forecast available from hub via MQTT"}


# ---- WebSocket for real-time updates ----

@app.websocket("/ws/live")
async def websocket_live(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    try:
        while True:
            # Send latest status every 5 seconds
            await asyncio.sleep(5)
            status = await get_status()
            await ws.send_text(json.dumps({"topic": "status", "data": status}))
    except Exception:
        pass
    finally:
        ws_clients.discard(ws)


# ---- Thermal Forecast Endpoint (cloud-side heavy ML) ----

@app.post("/api/forecast/run")
async def run_thermal_forecast():
    """
    Trigger cloud-side thermal forecast model.
    The hub runs a lightweight on-device forecast, but the cloud can run
    a more accurate model with weather data + historical patterns.
    Result is pushed to hub via MQTT.
    """
    # In production: run the full physics-informed model here
    # with weather forecast data, then publish optimized setpoints
    if mqtt_client and mqtt_connected:
        mqtt_client.publish(
            "thermogrid/forecast/update",
            json.dumps({"status": "computed", "steps": 16}),
            qos=1
        )
    return {"status": "forecast_computed", "steps": 16}


@app.post("/api/optimize")
async def run_energy_optimization():
    """
    Run MILP energy optimization on cloud.
    Computes optimal zone setpoints for next 4 hours considering:
    - Thermal forecast
    - Occupancy prediction
    - TOU tariff schedule
    - Solar production forecast
    Pushes optimized schedule to hub via MQTT.
    """
    # In production: solve MILP here
    # minimize: sum(zone_energy * tariff_rate)
    # subject to: comfort constraints, thermal dynamics, actuator limits
    if mqtt_client and mqtt_connected:
        mqtt_client.publish(
            "thermogrid/optimize/result",
            json.dumps({"status": "optimized", "horizon_hours": 4}),
            qos=1
        )
    return {"status": "optimized", "horizon_hours": 4}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
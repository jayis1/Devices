#!/usr/bin/env python3
"""
WanderSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

Endpoints:
  /api/v1/auth/login              — JWT login
  /api/v1/devices                 — List devices
  /api/v1/band/location           — Current Wander Band GPS location
  /api/v1/band/location/history   — GPS history
  /api/v1/band/health             — Band battery, HR, activity, wander risk
  /api/v1/band/geofence           — Geofence configuration
  /api/v1/doors                   — List Door Sentinels
  /api/v1/doors/{id}/lock         — Lock door
  /api/v1/doors/{id}/unlock       — Unlock door
  /api/v1/rooms                   — List Room Sentinels
  /api/v1/rooms/{id}/timeline     — 24-hour activity timeline
  /api/v1/adl/timeline            — Aggregated ADL timeline
  /api/v1/voice/reminders         — Reminder schedule
  /api/v1/voice/clips             — Voice clip management
  /api/v1/cognitive/score         — Cognitive decline score
  /api/v1/cognitive/trajectory    — 6-month trajectory
  /api/v1/cognitive/report        — Neurologist-ready PDF report
  /api/v1/anomalies               — Behavioral anomaly history
  /api/v1/anomalies/active        — Active anomaly
  /api/v1/sleep/summary           — Daily sleep summary
  /api/v1/sleep/weekly            — Weekly sleep report
  /api/v1/wander/events           — Wandering event history
  /api/v1/wander/active           — Active wandering event
  /api/v1/wander/route            — Predicted wandering route
  /api/v1/alerts                  — Alerts list
  /api/v1/dispatch/cancel         — Cancel 911 dispatch
  /api/v1/dispatch/status         — 911 dispatch status
  /api/v1/caregivers              — Caregiver management
  /api/v1/risk/wander             — Current wandering risk score
  /api/v1/risk/weekly             — Weekly care summary
  /api/v1/ws                      — Real-time WebSocket
"""
from __future__ import annotations

import asyncio
import json
import math
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel


# ─── Models ────────────────────────────────────────────────────────────────

class Device(BaseModel):
    device_id: str
    device_type: str  # hub, band, door, room, voice
    name: str
    firmware_version: str
    online: bool
    last_seen: datetime | None = None


class BandLocation(BaseModel):
    latitude: float
    longitude: float
    fix: int  # 0=no, 1=fix, 2=estimated
    timestamp: datetime
    activity: str
    wander_risk: int
    geofence_status: str  # inside, outside, near
    battery_v: float


class DoorStatus(BaseModel):
    door_id: int
    name: str
    door_state: str  # open, closed
    lock_state: str  # locked, unlocked, failed
    tamper: bool
    band_proximity: bool
    battery_v: float


class RoomStatus(BaseModel):
    room_id: int
    name: str
    presence: bool
    activity_class: str
    activity_confidence: int
    motion_level: int
    battery_v: float


class ReminderSlot(BaseModel):
    hour: int  # 0-23
    enabled: bool
    reminder_id: int
    clip_index: int
    volume: int
    reminder_type: str  # medication, meal, hydration, appointment, orientation, custom


class CognitiveScore(BaseModel):
    score: int  # 0-100 (0=stable, 100=severe decline)
    rate_per_month: float
    trend: str  # stable, declining, rapidly_declining
    last_updated: datetime
    mmse_equivalent: int | None = None


class AnomalyAlert(BaseModel):
    id: int
    timestamp: datetime
    anomaly_type: str
    severity: str
    description: str
    shap_factors: list[dict]
    recommendation: str
    resolved: bool = False


class WanderingEvent(BaseModel):
    id: int
    timestamp: datetime
    latitude: float
    longitude: float
    geofence_breach: bool
    resolved: bool
    resolution: str | None = None
    duration_min: int | None = None


class Alert(BaseModel):
    id: int
    timestamp: datetime
    alert_type: str  # wander, fall, sos, door, anomaly, battery, offline
    severity: str  # info, warning, critical, emergency
    message: str
    acknowledged: bool = False


class GeofenceConfig(BaseModel):
    center_lat: float
    center_lon: float
    radius_m: int
    night_radius_m: int
    night_start_h: int
    night_end_h: int


class Caregiver(BaseModel):
    id: str
    name: str
    email: str
    role: str  # family, professional, neurologist
    permissions: list[str]


# ─── Constants ──────────────────────────────────────────────────────────────

ACTIVITY_NAMES = {
    0: "absent", 1: "walking", 2: "sitting", 3: "lying",
    4: "eating", 5: "cooking", 6: "pacing", 7: "standing",
}

BAND_ACTIVITY_NAMES = {
    0: "sitting", 1: "walking", 2: "lying", 3: "standing",
    4: "fidgeting", 5: "fall",
}

SEVERITY_MAP = {0: "info", 1: "warning", 2: "critical", 3: "emergency"}

REMINDER_TYPES = {
    0: "medication", 1: "meal", 2: "hydration",
    3: "appointment", 4: "orientation", 5: "custom",
}


# ─── In-memory stores (production: PostgreSQL + InfluxDB) ──────────────────

class DataStore:
    def __init__(self) -> None:
        self.devices: dict[str, Device] = {}
        self.band_locations: list[dict[str, Any]] = []
        self.doors: dict[int, DoorStatus] = {}
        self.rooms: dict[int, RoomStatus] = {}
        self.reminders: dict[int, ReminderSlot] = {}
        self.cognitive_score: CognitiveScore = CognitiveScore(
            score=15, rate_per_month=0.5, trend="stable",
            last_updated=datetime.now(timezone.utc), mmse_equivalent=24
        )
        self.anomalies: list[dict[str, Any]] = []
        self.wander_events: list[dict[str, Any]] = []
        self.alerts: list[dict[str, Any]] = []
        self.caregivers: list[dict[str, Any]] = []
        self.geofence = GeofenceConfig(
            center_lat=37.4449, center_lon=-122.4159,
            radius_m=200, night_radius_m=100,
            night_start_h=22, night_end_h=6
        )
        self.active_wander: dict | None = None
        self.active_anomaly: dict | None = None
        self.dispatch_active = False
        self.sleep_summary = {
            "quality_score": 72,
            "total_sleep_min": 384,
            "deep_sleep_min": 68,
            "rem_sleep_min": 72,
            "light_sleep_min": 168,
            "wake_count": 3,
            "circadian_disruption": 28,
            "nighttime_wandering": False,
        }

        # Seed default doors
        for i, name in enumerate(["Front Door", "Back Door", "Garage Door", "Patio Door"]):
            self.doors[i] = DoorStatus(
                door_id=i, name=name, door_state="closed",
                lock_state="locked" if i < 2 else "unlocked",
                tamper=False, band_proximity=False, battery_v=4.8
            )

        # Seed default rooms
        for i, name in enumerate(["Living Room", "Kitchen", "Bedroom", "Bathroom"]):
            self.rooms[i] = RoomStatus(
                room_id=i, name=name, presence=(i == 0),
                activity_class="sitting" if i == 0 else "absent",
                activity_confidence=85 if i == 0 else 90,
                motion_level=45 if i == 0 else 0,
                battery_v=4.15
            )

        # Seed default reminders
        self.reminders[8] = ReminderSlot(hour=8, enabled=True, reminder_id=1,
                                          clip_index=0, volume=80,
                                          reminder_type="medication")
        self.reminders[12] = ReminderSlot(hour=12, enabled=True, reminder_id=2,
                                           clip_index=20, volume=80,
                                           reminder_type="meal")
        self.reminders[18] = ReminderSlot(hour=18, enabled=True, reminder_id=3,
                                           clip_index=1, volume=80,
                                           reminder_type="medication")
        self.reminders[19] = ReminderSlot(hour=19, enabled=True, reminder_id=4,
                                           clip_index=21, volume=80,
                                           reminder_type="meal")

        # Seed caregivers
        self.caregivers.append({
            "id": "1", "name": "Sarah Johnson", "email": "sarah@example.com",
            "role": "family", "permissions": ["read", "control", "alerts"]
        })

    def add_alert(self, alert_type: str, severity: str, message: str) -> None:
        self.alerts.append({
            "id": len(self.alerts),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "alert_type": alert_type,
            "severity": severity,
            "message": message,
            "acknowledged": False,
        })


store = DataStore()
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")


# ─── MQTT Bridge (simulated) ────────────────────────────────────────────────

class MQTTPublisher:
    def __init__(self) -> None:
        self.connected = False

    async def connect(self) -> None:
        self.connected = True
        print("[MQTT] Connected to broker")

    async def publish(self, topic: str, payload: dict) -> None:
        print(f"[MQTT] pub {topic}: {json.dumps(payload)[:120]}")

    async def publish_command(self, device_id: str, command: str, data: dict) -> None:
        topic = f"wandersync/default/cloud/command"
        await self.publish(topic, {"device": device_id, "command": command, **data})


mqtt = MQTTPublisher()


# ─── WebSocket Connection Manager ──────────────────────────────────────────

class ConnectionManager:
    def __init__(self) -> None:
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket) -> None:
        if ws in self.active:
            self.active.remove(ws)

    async def broadcast(self, message: dict) -> None:
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                self.disconnect(ws)


manager = ConnectionManager()


# ─── App ────────────────────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    await mqtt.connect()
    asyncio.create_task(telemetry_simulator())
    yield


app = FastAPI(title="WanderSync", version="1.0.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


async def telemetry_simulator() -> None:
    """Simulate periodic telemetry. Production: MQTT subscriber."""
    while True:
        await asyncio.sleep(30)
        await manager.broadcast({
            "type": "heartbeat",
            "ts": datetime.now(timezone.utc).isoformat()
        })


# ─── Auth ───────────────────────────────────────────────────────────────────

@app.post("/api/v1/auth/login")
async def login(username: str = "demo", password: str = "demo"):
    from jose import jwt as jose_jwt
    import os
    token = jose_jwt.encode(
        {"sub": username, "exp": int(time.time()) + 86400},
        os.environ.get("JWT_SECRET", "wandersync-dev-secret"),
        algorithm="HS256",
    )
    return {"access_token": token, "token_type": "bearer"}


# ─── Devices ────────────────────────────────────────────────────────────────

@app.get("/api/v1/devices")
async def list_devices():
    return [
        {"device_id": "hub-001", "device_type": "hub", "name": "Care Hub",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "band-001", "device_type": "band", "name": "Wander Band",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "door-001", "device_type": "door", "name": "Front Door",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "door-002", "device_type": "door", "name": "Back Door",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "room-001", "device_type": "room", "name": "Living Room Sentinel",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "room-002", "device_type": "room", "name": "Kitchen Sentinel",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "voice-001", "device_type": "voice", "name": "Voice Node",
         "firmware_version": "1.0.0", "online": True},
    ]


@app.post("/api/v1/devices/{device_id}/ota")
async def trigger_ota(device_id: str, version: str = "1.1.0"):
    await mqtt.publish_command(device_id, "ota", {"version": version})
    return {"status": "ota_triggered", "device": device_id, "version": version}


# ─── Wander Band ────────────────────────────────────────────────────────────

@app.get("/api/v1/band/location", response_model=BandLocation)
async def get_band_location():
    return BandLocation(
        latitude=37.4449 + (time.time() % 100) * 0.0001,
        longitude=-122.4159 + (time.time() % 50) * 0.0001,
        fix=1, timestamp=datetime.now(timezone.utc),
        activity="sitting", wander_risk=15,
        geofence_status="inside", battery_v=4.15
    )


@app.get("/api/v1/band/location/history")
async def get_band_history(start: str | None = None, end: str | None = None,
                            limit: int = 100):
    return store.band_locations[-limit:] if store.band_locations else []


@app.get("/api/v1/band/health")
async def get_band_health():
    return {
        "battery_v": 4.15, "battery_pct": 82,
        "heart_rate_bpm": 72, "hrv_ms": 45,
        "activity": "sitting", "wander_risk": 15,
        "steps_today": 3420, "steps_7d": 24800,
        "band_on_wrist": True,
        "geofence_status": "inside",
        "distance_home_m": 12,
    }


@app.get("/api/v1/band/geofence", response_model=GeofenceConfig)
async def get_geofence():
    return store.geofence


@app.post("/api/v1/band/geofence")
async def update_geofence(gf: GeofenceConfig):
    store.geofence = gf
    await mqtt.publish_command("band-001", "set_geofence", gf.model_dump())
    return {"status": "geofence_updated", "config": gf}


# ─── Doors ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/doors")
async def list_doors():
    return list(store.doors.values())


@app.get("/api/v1/doors/{door_id}")
async def get_door(door_id: int):
    if door_id not in store.doors:
        raise HTTPException(404, "Door not found")
    return store.doors[door_id]


@app.post("/api/v1/doors/{door_id}/lock")
async def lock_door(door_id: int):
    if door_id not in store.doors:
        raise HTTPException(404, "Door not found")
    await mqtt.publish_command(f"door-{door_id:03d}", "lock", {"door_id": door_id})
    store.doors[door_id].lock_state = "locked"
    await manager.broadcast({"type": "door_locked", "door_id": door_id})
    return {"status": "locked", "door_id": door_id}


@app.post("/api/v1/doors/{door_id}/unlock")
async def unlock_door(door_id: int):
    if door_id not in store.doors:
        raise HTTPException(404, "Door not found")
    await mqtt.publish_command(f"door-{door_id:03d}", "unlock", {"door_id": door_id})
    store.doors[door_id].lock_state = "unlocked"
    await manager.broadcast({"type": "door_unlocked", "door_id": door_id})
    return {"status": "unlocked", "door_id": door_id}


@app.post("/api/v1/doors/lock-all")
async def lock_all_doors():
    for door_id in store.doors:
        store.doors[door_id].lock_state = "locked"
    await mqtt.publish_command("hub-001", "lock_all", {})
    return {"status": "all_locked", "count": len(store.doors)}


# ─── Rooms ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/rooms")
async def list_rooms():
    return list(store.rooms.values())


@app.get("/api/v1/rooms/{room_id}")
async def get_room(room_id: int):
    if room_id not in store.rooms:
        raise HTTPException(404, "Room not found")
    return store.rooms[room_id]


@app.get("/api/v1/rooms/{room_id}/timeline")
async def get_room_timeline(room_id: int):
    """24-hour activity timeline for a room."""
    timeline = []
    for hour in range(24):
        activities = ["absent", "sleeping", "sleeping", "sleeping", "sleeping",
                      "sleeping", "absent", "sitting", "cooking", "absent",
                      "walking", "sitting", "eating", "sitting", "absent",
                      "walking", "sitting", "sitting", "cooking", "eating",
                      "sitting", "walking", "sitting", "lying"]
        timeline.append({"hour": hour, "activity": activities[hour]})
    return timeline


@app.get("/api/v1/adl/timeline")
async def get_adl_timeline():
    """Aggregated ADL timeline across all rooms."""
    return {
        "date": datetime.now(timezone.utc).date().isoformat(),
        "summary": {
            "sleep_hours": 6.4,
            "meals": 3,
            "cooking_events": 2,
            "pacing_events": 1,
            "outdoor_excursions": 1,
            "room_transitions": 47,
            "medication_adherence": 1.0,
        },
        "hourly": [
            {"hour": h, "primary_activity": "sleep" if h < 6 or h > 22
             else "sit" if h in [7, 9, 10, 14, 15, 16, 20, 21]
             else "walk" if h in [11, 13, 17]
             else "cook" if h in [8, 18]
             else "eat" if h in [12, 19] else "absent"}
            for h in range(24)
        ],
    }


# ─── Voice Reminders ────────────────────────────────────────────────────────

@app.get("/api/v1/voice/reminders")
async def get_reminders():
    return list(store.reminders.values())


@app.post("/api/v1/voice/reminders")
async def add_reminder(slot: ReminderSlot):
    store.reminders[slot.hour] = slot
    await mqtt.publish_command("voice-001", "set_reminder", slot.model_dump())
    return {"status": "reminder_set", "reminder": slot}


@app.post("/api/v1/voice/reminders/{reminder_id}/trigger")
async def trigger_reminder(reminder_id: int):
    await mqtt.publish_command("voice-001", "trigger_reminder",
                                {"reminder_id": reminder_id})
    return {"status": "reminder_triggered", "reminder_id": reminder_id}


@app.get("/api/v1/voice/clips")
async def list_clips():
    clips = []
    categories = [
        ("Medication", 0, 20), ("Meals", 20, 20), ("Hydration", 40, 10),
        ("Appointments", 50, 10), ("Orientation", 60, 10),
        ("Safety", 70, 10), ("Social", 80, 10), ("Comfort", 90, 10),
        ("Time", 100, 10), ("Emergency", 110, 10),
    ]
    for cat, start, count in categories:
        for i in range(count):
            clips.append({"index": start + i, "category": cat,
                          "duration_s": 10, "uploaded": True})
    return clips


# ─── Cognitive Health ───────────────────────────────────────────────────────

@app.get("/api/v1/cognitive/score", response_model=CognitiveScore)
async def get_cognitive_score():
    return store.cognitive_score


@app.get("/api/v1/cognitive/trajectory")
async def get_cognitive_trajectory(months: int = 6):
    """6-month cognitive decline trajectory."""
    trajectory = []
    for m in range(months, 0, -1):
        trajectory.append({
            "month": m,
            "score": max(5, 15 - m * 0.8),
            "mmse_equivalent": max(18, 26 - m * 0.5),
            "key_changes": "Stable" if m > 4 else "Slight decline in cooking frequency"
        })
    return trajectory


@app.get("/api/v1/cognitive/report")
async def get_cognitive_report():
    """Neurologist-ready PDF report (simplified — returns summary)."""
    return {
        "report_type": "Cognitive Assessment Report",
        "generated": datetime.now(timezone.utc).isoformat(),
        "patient_summary": {
            "cognitive_score": 15,
            "trend": "stable",
            "rate_per_month": 0.5,
            "mmse_equivalent": 24,
        },
        "adl_analysis": {
            "cooking_frequency": "2x daily (normal)",
            "eating_regularity": "Regular meal times (normal)",
            "sleep_pattern": "6.4 hours average, 3 nighttime wakings",
            "pacing_events": "1-2x weekly (within normal range)",
            "medication_adherence": "100% (ReminderOpt active)",
            "social_interaction": "Voice node interaction 4x daily",
        },
        "anomaly_history": "No significant anomalies in last 30 days",
        "recommendation": "Continue current care plan. Re-assess in 3 months.",
    }


# ─── Anomalies ──────────────────────────────────────────────────────────────

@app.get("/api/v1/anomalies")
async def get_anomalies(limit: int = 50):
    return store.anomalies[-limit:]


@app.get("/api/v1/anomalies/active")
async def get_active_anomaly():
    return store.active_anomaly or {"active": False}


# ─── Sleep ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/sleep/summary")
async def get_sleep_summary():
    return store.sleep_summary


@app.get("/api/v1/sleep/weekly")
async def get_sleep_weekly():
    days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    return {
        "days": [
            {"day": d, "quality": 70 + i * 2, "duration_h": 6.0 + i * 0.2,
             "nighttime_wandering": False, "circadian_disruption": 25 - i * 2}
            for i, d in enumerate(days)
        ],
        "weekly_avg": {"quality": 76, "duration_h": 6.5,
                        "circadian_disruption": 21},
    }


# ─── Wandering Events ───────────────────────────────────────────────────────

@app.get("/api/v1/wander/events")
async def get_wander_events(limit: int = 50):
    return store.wander_events[-limit:]


@app.get("/api/v1/wander/active")
async def get_active_wander():
    return store.active_wander or {"active": False}


@app.post("/api/v1/wander/{event_id}/resolve")
async def resolve_wander(event_id: int, resolution: str = "found_safe"):
    for e in store.wander_events:
        if e.get("id") == event_id:
            e["resolved"] = True
            e["resolution"] = resolution
            store.active_wander = None
            await manager.broadcast({"type": "wander_resolved",
                                      "event_id": event_id})
            return {"status": "resolved", "event_id": event_id}
    raise HTTPException(404, "Wandering event not found")


@app.get("/api/v1/wander/route")
async def get_wander_route():
    if not store.active_wander:
        return {"active": False}
    return {
        "active": True,
        "predicted_route": [
            {"lat": 37.4449, "lon": -122.4159, "confidence": 0.9},
            {"lat": 37.4451, "lon": -122.4160, "confidence": 0.85},
            {"lat": 37.4453, "lon": -122.4162, "confidence": 0.78},
        ],
        "predicted_destination": "Nearby park (240m NE)",
        "mean_displacement_error_15min_m": 85,
    }


# ─── Alerts ─────────────────────────────────────────────────────────────────

@app.get("/api/v1/alerts")
async def get_alerts(limit: int = 50, alert_type: str | None = None):
    alerts = store.alerts[-limit:]
    if alert_type:
        alerts = [a for a in alerts if a["alert_type"] == alert_type]
    return alerts


@app.put("/api/v1/alerts/{alert_id}/ack")
async def ack_alert(alert_id: int):
    for a in store.alerts:
        if a.get("id") == alert_id:
            a["acknowledged"] = True
            return {"status": "acknowledged"}
    raise HTTPException(404, "Alert not found")


# ─── Emergency Dispatch ─────────────────────────────────────────────────────

@app.post("/api/v1/dispatch/cancel")
async def cancel_dispatch():
    store.dispatch_active = False
    await manager.broadcast({"type": "dispatch_cancelled"})
    return {"status": "dispatch_cancelled"}


@app.get("/api/v1/dispatch/status")
async def get_dispatch_status():
    return {"active": store.dispatch_active, "timestamp": datetime.now(timezone.utc)}


# ─── Caregivers ─────────────────────────────────────────────────────────────

@app.get("/api/v1/caregivers")
async def list_caregivers():
    return store.caregivers


@app.post("/api/v1/caregivers")
async def invite_caregiver(name: str, email: str, role: str = "family"):
    caregiver = {
        "id": str(len(store.caregivers) + 1),
        "name": name, "email": email, "role": role,
        "permissions": ["read", "alerts"] if role == "family" else ["read"]
    }
    store.caregivers.append(caregiver)
    return caregiver


# ─── Risk ───────────────────────────────────────────────────────────────────

@app.get("/api/v1/risk/wander")
async def get_wander_risk():
    return {
        "score": 15,
        "level": "low",
        "factors": [
            {"feature": "geofence_status", "contribution": 0, "value": "inside"},
            {"feature": "time_of_day", "contribution": 0, "value": "afternoon"},
            {"feature": "activity", "contribution": 5, "value": "sitting"},
            {"feature": "historical_wander_7d", "contribution": 0, "value": "0 events"},
            {"feature": "sleep_quality", "contribution": 10, "value": "below average"},
        ],
        "recommendation": "Wandering risk is LOW. Sleep quality slightly below average — monitor for sundowning."
    }


@app.get("/api/v1/risk/weekly")
async def get_weekly_summary():
    return {
        "week": "2026-W32",
        "summary": {
            "wandering_events": 0,
            "fall_events": 0,
            "sos_presses": 0,
            "door_alerts": 2,
            "behavioral_anomalies": 0,
            "medication_adherence": 1.0,
            "avg_sleep_quality": 76,
            "avg_wander_risk": 18,
            "cognitive_trend": "stable",
            "caregiver_alerts": 2,
        },
        "highlights": [
            "No wandering events this week",
            "100% medication adherence (ReminderOpt working well)",
            "Sleep quality improved 8% from last week",
            "2 door alerts (front door opened at 10:15 PM — auto-locked, safe)",
        ],
    }


# ─── WebSocket ──────────────────────────────────────────────────────────────

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            data = await ws.receive_text()
            # Echo or handle client messages
            msg = json.loads(data) if data else {}
            if msg.get("type") == "ping":
                await ws.send_json({"type": "pong"})
    except WebSocketDisconnect:
        manager.disconnect(ws)
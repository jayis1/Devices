from __future__ import annotations

import asyncio
from collections import defaultdict
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, WebSocket
from fastapi.middleware.cors import CORSMiddleware

try:
    import paho.mqtt.client as mqtt
except Exception:  # pragma: no cover
    mqtt = None

from models import ReliefCommand, SymptomEvent, TelemetryIn
from ml_inference import dashboard_risk

app = FastAPI(title="PeriodSync API", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

STATE: dict[str, Any] = {
    "telemetry": defaultdict(list),
    "symptoms": defaultdict(list),
    "commands": defaultdict(list),
    "mqtt_connected": False,
}


def _maybe_start_mqtt() -> None:
    if mqtt is None:
        return

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(client, userdata, flags, reason_code, properties):
        STATE["mqtt_connected"] = reason_code == 0
        if STATE["mqtt_connected"]:
            client.subscribe("periodsync/+/telemetry/+")

    def on_message(client, userdata, msg):
        STATE.setdefault("mqtt_last_topic", msg.topic)

    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect_async("localhost", 1883, 60)
        client.loop_start()
    except Exception:
        STATE["mqtt_connected"] = False


@app.on_event("startup")
async def startup() -> None:
    _maybe_start_mqtt()


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "time": datetime.now(timezone.utc).isoformat(),
        "users": len(STATE["telemetry"]),
        "mqtt_connected": STATE["mqtt_connected"],
        "models": ["PhaseCast", "FlowGuard", "CrampAhead", "IronWatch", "ReliefTune"],
    }


@app.post("/telemetry")
async def ingest_telemetry(payload: TelemetryIn) -> dict[str, Any]:
    record = payload.model_dump(mode="json")
    STATE["telemetry"][payload.user_id].append(record)
    return {"accepted": True, "count": len(STATE["telemetry"][payload.user_id])}


@app.post("/symptoms")
async def ingest_symptom(payload: SymptomEvent) -> dict[str, Any]:
    record = payload.model_dump(mode="json")
    STATE["symptoms"][payload.user_id].append(record)
    return {"accepted": True, "count": len(STATE["symptoms"][payload.user_id])}


@app.post("/commands/relief-belt")
async def command_relief_belt(payload: ReliefCommand) -> dict[str, Any]:
    command = payload.model_dump(mode="json")
    command["queued_at"] = datetime.now(timezone.utc).isoformat()
    STATE["commands"][payload.user_id].append(command)
    return {"queued": True, "command": command}


@app.get("/users/{user_id}/risk")
async def user_risk(user_id: str) -> dict[str, Any]:
    latest = {}
    if STATE["telemetry"][user_id]:
        latest = STATE["telemetry"][user_id][-1].get("metrics", {})
    symptoms = STATE["symptoms"][user_id]
    return dashboard_risk(latest, symptoms)


@app.get("/users/{user_id}/dashboard")
async def dashboard(user_id: str) -> dict[str, Any]:
    latest = STATE["telemetry"][user_id][-1] if STATE["telemetry"][user_id] else None
    risk = await user_risk(user_id)
    return {
        "user_id": user_id,
        "latest_telemetry": latest,
        "risk": risk,
        "queued_commands": STATE["commands"][user_id][-5:],
        "recent_symptoms": STATE["symptoms"][user_id][-5:],
    }


@app.get("/reports/{user_id}")
async def reports(user_id: str) -> dict[str, Any]:
    risk = await user_risk(user_id)
    return {
        "user_id": user_id,
        "summary": {
            "telemetry_points": len(STATE["telemetry"][user_id]),
            "symptom_events": len(STATE["symptoms"][user_id]),
            "top_flag": max(risk, key=risk.get) if risk else None,
        },
        "risk": risk,
    }


@app.websocket("/ws/{user_id}")
async def ws(user_id: str, websocket: WebSocket) -> None:
    await websocket.accept()
    for _ in range(3):
        await websocket.send_json(await dashboard(user_id))
        await asyncio.sleep(1)
    await websocket.close()

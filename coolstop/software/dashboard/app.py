import json
import os
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, ConfigDict, Field, field_validator

DB_PATH = Path(os.getenv("COOLSTOP_DB", "coolstop.sqlite3"))
app = FastAPI(title="CoolStop local readiness API", version="0.1.0")


class Telemetry(BaseModel):
    model_config = ConfigDict(extra="forbid")
    version: int
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0)
    timestamp: datetime
    kind: str
    values: dict[str, Any]

    @field_validator("version")
    @classmethod
    def supported_version(cls, value: int) -> int:
        if value != 1:
            raise ValueError("unsupported protocol version")
        return value

    @field_validator("timestamp")
    @classmethod
    def utc_timestamp(cls, value: datetime) -> datetime:
        if value.tzinfo is None:
            raise ValueError("timestamp must include timezone")
        return value

def validate_kind(item: Telemetry) -> None:
    limits = {"climate": {"temperature_c": (-20, 80), "relative_humidity_pct": (0, 100)},
              "supply": {"mass_g": (0, 100000)}}
    if item.kind not in limits:
        raise HTTPException(status_code=422, detail="unsupported kind")
    expected = limits[item.kind]
    if set(item.values) != set(expected):
        raise HTTPException(status_code=422, detail="invalid value fields")
    for key, (low, high) in expected.items():
        value = item.values[key]
        if isinstance(value, bool) or not isinstance(value, (float, int)) or not low <= value <= high:
            raise HTTPException(status_code=422, detail=f"invalid {key}")


def connection() -> sqlite3.Connection:
    db = sqlite3.connect(DB_PATH)
    db.execute("CREATE TABLE IF NOT EXISTS telemetry (node_id TEXT PRIMARY KEY, seq INTEGER NOT NULL, payload TEXT NOT NULL)")
    return db


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/telemetry")
def post_telemetry(item: Telemetry) -> dict[str, bool]:
    validate_kind(item)
    with connection() as db:
        db.execute("INSERT INTO telemetry(node_id,seq,payload) VALUES(?,?,?) ON CONFLICT(node_id) DO UPDATE SET seq=excluded.seq,payload=excluded.payload",
                   (item.node_id, item.seq, item.model_dump_json()))
    return {"accepted": True}


@app.get("/telemetry/latest")
def latest() -> list[dict[str, Any]]:
    with connection() as db:
        rows = db.execute("SELECT payload FROM telemetry ORDER BY node_id").fetchall()
    return [json.loads(row[0]) for row in rows]


# Optional MQTT integration; use unique broker credentials/ACLs and TLS in deployment.
def mqtt_callback(_client: Any, _userdata: Any, message: Any) -> None:
    if len(message.payload) > 1024:
        return
    try:
        item = Telemetry.model_validate_json(message.payload)
        post_telemetry(item)
    except Exception:
        return


def run_mqtt() -> None:
    import paho.mqtt.client as mqtt
    host = os.environ["MQTT_HOST"]
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = mqtt_callback
    if os.getenv("MQTT_USERNAME"):
        client.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
    client.connect(host, int(os.getenv("MQTT_PORT", "1883")))
    client.subscribe("coolstop/v1/+/+/telemetry", qos=1)
    client.loop_forever()

"""
QuakeGuard Cloud Backend — FastAPI + MQTT + TimescaleDB

Endpoints:
  GET  /events          — list seismic events
  GET  /events/{id}     — event detail with waveforms
  GET  /nodes           — list all nodes + status
  GET  /structural      — structural health reports
  POST /family/response — family check-in response
  GET  /reports         — generate PDF structural report
  WS   /ws              — WebSocket for real-time alerts

MQTT topics:
  quakeguard/+/event         — seismic events
  quakeguard/+/node/+/status — node heartbeats
  quakeguard/+/structural/+  — structural reports
  quakeguard/+/family/+      — family responses

License: MIT
"""
import os
import json
import asyncio
from datetime import datetime, timezone
from typing import Optional
from contextlib import asynccontextmanager

import aiomqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
import asyncpg
import structlog

# ── Configuration ──────────────────────────────────────────────

MQTT_BROKER = os.getenv("MQTT_BROKER", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
DB_DSN = os.getenv("DB_DSN", "postgres://quakeguard:quakeguard@db:5432/quakeguard")
FIREBASE_API_KEY = os.getenv("FIREBASE_API_KEY", "")

log = structlog.get_logger()

# ── Database ───────────────────────────────────────────────────

db_pool: Optional[asyncpg.Pool] = None
ws_clients: set[WebSocket] = set()


async def init_db():
    """Initialize database schema (idempotent)."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS nodes (
                id SERIAL PRIMARY KEY,
                hub_id TEXT NOT NULL,
                node_addr INTEGER NOT NULL,
                node_type TEXT NOT NULL,  -- hub, floor, shutoff, structural
                battery_pct INTEGER DEFAULT 100,
                temperature_c REAL DEFAULT 0,
                status TEXT DEFAULT 'offline',
                last_seen TIMESTAMPTZ DEFAULT NOW(),
                UNIQUE(hub_id, node_addr)
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id SERIAL PRIMARY KEY,
                hub_id TEXT NOT NULL,
                event_id INTEGER NOT NULL,
                timestamp TIMESTAMPTZ NOT NULL,
                severity INTEGER DEFAULT 0,
                magnitude_x10 INTEGER DEFAULT 0,
                epicenter_dist_km INTEGER DEFAULT 0,
                actions_taken INTEGER DEFAULT 0,
                node_count INTEGER DEFAULT 0,
                usgs_id TEXT,
                created_at TIMESTAMPTZ DEFAULT NOW()
            );
        """)
        await conn.execute("""
            SELECT create_hypertable('events', 'timestamp',
                if_not_exists => TRUE);
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS waveforms (
                id SERIAL PRIMARY KEY,
                event_id INTEGER REFERENCES events(id),
                node_addr INTEGER NOT NULL,
                axis_flags INTEGER DEFAULT 0,
                sample_rate_khz INTEGER DEFAULT 1,
                data BYTEA,
                created_at TIMESTAMPTZ DEFAULT NOW()
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS structural_reports (
                id SERIAL PRIMARY KEY,
                hub_id TEXT NOT NULL,
                node_addr INTEGER NOT NULL,
                event_id INTEGER,
                strain_max_micro INTEGER DEFAULT 0,
                strain_mean_micro INTEGER DEFAULT 0,
                resonance_shift_hz INTEGER DEFAULT 0,
                peak_accel_mg INTEGER DEFAULT 0,
                temperature_c10 INTEGER DEFAULT 0,
                anomaly_score INTEGER DEFAULT 0,
                timestamp TIMESTAMPTZ DEFAULT NOW()
            );
        """)
        await conn.execute("""
            SELECT create_hypertable('structural_reports', 'timestamp',
                if_not_exists => TRUE);
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS family_responses (
                id SERIAL PRIMARY KEY,
                hub_id TEXT NOT NULL,
                event_id INTEGER NOT NULL,
                user_id TEXT NOT NULL,
                status TEXT NOT NULL,  -- safe, need_help, no_response
                timestamp TIMESTAMPTZ DEFAULT NOW()
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS gas_readings (
                id SERIAL PRIMARY KEY,
                hub_id TEXT NOT NULL,
                h2_ppm INTEGER DEFAULT 0,
                ch4_ppm INTEGER DEFAULT 0,
                temperature_c REAL DEFAULT 0,
                relay_states INTEGER DEFAULT 0,
                timestamp TIMESTAMPTZ DEFAULT NOW()
            );
        """)
        await conn.execute("""
            SELECT create_hypertable('gas_readings', 'timestamp',
                if_not_exists => TRUE);
        """)


# ── FastAPI App ────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    db_pool = await asyncpg.create_pool(DB_DSN, min_size=2, max_size=10)
    await init_db()
    # Start MQTT subscriber task
    mqtt_task = asyncio.create_task(mqtt_subscriber())
    yield
    mqtt_task.cancel()
    await db_pool.close()

app = FastAPI(
    title="QuakeGuard Cloud API",
    version="1.0.0",
    lifespan=lifespan,
)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


# ── Pydantic Models ───────────────────────────────────────────

class FamilyResponse(BaseModel):
    hub_id: str
    event_id: int
    user_id: str
    status: str = Field(..., pattern="^(safe|need_help|no_response)$")


class NodeStatus(BaseModel):
    hub_id: str
    node_addr: int
    node_type: str
    battery_pct: int
    temperature_c: float
    status: str
    last_seen: datetime


class SeismicEvent(BaseModel):
    id: int
    hub_id: str
    event_id: int
    timestamp: datetime
    severity: int
    magnitude: float
    epicenter_dist_km: int
    actions_taken: int
    node_count: int


# ── MQTT Subscriber ───────────────────────────────────────────

async def mqtt_subscriber():
    """Subscribe to QuakeGuard MQTT topics and persist to DB."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                await client.subscribe("quakeguard/+/event")
                await client.subscribe("quakeguard/+/node/+/status")
                await client.subscribe("quakeguard/+/structural/+")
                await client.subscribe("quakeguard/+/family/+")
                await client.subscribe("quakeguard/+/gas/+")
                log.info("mqtt_connected", broker=MQTT_BROKER)

                async for message in client.messages:
                    topic_parts = str(message.topic).split("/")
                    payload = json.loads(message.payload.decode())

                    if "event" in str(message.topic):
                        await handle_event(topic_parts[1], payload)
                    elif "status" in str(message.topic):
                        await handle_status(topic_parts[1], topic_parts[3], payload)
                    elif "structural" in str(message.topic):
                        await handle_structural(topic_parts[1], topic_parts[3], payload)
                    elif "family" in str(message.topic):
                        await handle_family(topic_parts[1], topic_parts[3], payload)
                    elif "gas" in str(message.topic):
                        await handle_gas(topic_parts[1], payload)

        except Exception as e:
            log.error("mqtt_error", error=str(e))
            await asyncio.sleep(5)


async def handle_event(hub_id: str, payload: dict):
    """Handle seismic event from Hub."""
    async with db_pool.acquire() as conn:
        event_id = await conn.fetchval("""
            INSERT INTO events (hub_id, event_id, timestamp, severity,
                               magnitude_x10, epicenter_dist_km,
                               actions_taken, node_count)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
            RETURNING id
        """, hub_id,
            payload["event_id"],
            datetime.fromtimestamp(payload["timestamp_utc"], tz=timezone.utc),
            payload["severity"],
            payload["magnitude_x10"],
            payload["epicenter_dist_km"],
            payload["actions_taken"],
            payload["node_count"]
        )
    log.info("event_stored", hub_id=hub_id, event_id=event_id,
             magnitude=payload["magnitude_x10"] / 10)

    # Broadcast to WebSocket clients
    await broadcast_ws({
        "type": "event",
        "hub_id": hub_id,
        "event_id": event_id,
        "severity": payload["severity"],
        "magnitude": payload["magnitude_x10"] / 10,
        "actions_taken": payload["actions_taken"],
        "timestamp": payload["timestamp_utc"],
    })

    # Dispatch family check-in push notifications
    await dispatch_family_checkin(hub_id, event_id, payload)


async def handle_status(hub_id: str, node_addr: str, payload: dict):
    """Handle node heartbeat."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            INSERT INTO nodes (hub_id, node_addr, node_type, battery_pct,
                             temperature_c, status, last_seen)
            VALUES ($1, $2, $3, $4, $5, 'online', NOW())
            ON CONFLICT (hub_id, node_addr) DO UPDATE
            SET battery_pct = $4, temperature_c = $5,
                status = 'online', last_seen = NOW()
        """, hub_id, int(node_addr, 16),
            payload.get("node_type", "unknown"),
            payload["battery_pct"],
            payload["temperature_c"] / 10.0
        )


async def handle_structural(hub_id: str, node_addr: str, payload: dict):
    """Handle structural health report from Structural Tag."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            INSERT INTO structural_reports (hub_id, node_addr, event_id,
                strain_max_micro, strain_mean_micro, resonance_shift_hz,
                peak_accel_mg, temperature_c10, anomaly_score)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
        """, hub_id, int(node_addr, 16), payload.get("event_id"),
            payload["strain_max_micro"],
            payload["strain_mean_micro"],
            payload["resonance_shift_hz"],
            payload["peak_accel_mg"],
            payload["temperature_c10"],
            payload["anomaly_score"]
        )
    log.info("structural_report", hub_id=hub_id,
             node=node_addr,
             strain_max=payload["strain_max_micro"],
             anomaly=payload["anomaly_score"])


async def handle_family(hub_id: str, user_id: str, payload: dict):
    """Handle family check-in response from mobile app."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            INSERT INTO family_responses (hub_id, event_id, user_id, status)
            VALUES ($1, $2, $3, $4)
        """, hub_id, payload["event_id"], user_id, payload["status"])
    log.info("family_response", hub_id=hub_id,
             user=user_id, status=payload["status"])


async def handle_gas(hub_id: str, payload: dict):
    """Handle post-shutoff gas readings from Shutoff Controller."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            INSERT INTO gas_readings (hub_id, h2_ppm, ch4_ppm,
                temperature_c, relay_states)
            VALUES ($1, $2, $3, $4, $5)
        """, hub_id, payload["h2_ppm"], payload["ch4_ppm"],
            payload["temperature_c"] / 10.0, payload["relay_states"])

    # Alert if gas leak detected
    if payload["h2_ppm"] > 100 or payload["ch4_ppm"] > 100:
        await broadcast_ws({
            "type": "gas_leak",
            "hub_id": hub_id,
            "h2_ppm": payload["h2_ppm"],
            "ch4_ppm": payload["ch4_ppm"],
        })


async def dispatch_family_checkin(hub_id: str, event_id: int, payload: dict):
    """Send Firebase push notifications to all family members."""
    # In production: integrate Firebase Admin SDK
    # For now: broadcast via WebSocket and log
    await broadcast_ws({
        "type": "family_checkin",
        "hub_id": hub_id,
        "event_id": event_id,
        "question": "Earthquake detected. Are you safe?",
        "magnitude": payload["magnitude_x10"] / 10,
        "actions_taken": payload["actions_taken"],
    })
    log.info("family_checkin_dispatched", hub_id=hub_id, event_id=event_id)


# ── WebSocket ──────────────────────────────────────────────────

async def broadcast_ws(message: dict):
    """Broadcast message to all connected WebSocket clients."""
    dead = set()
    for ws in ws_clients:
        try:
            await ws.send_json(message)
        except Exception:
            dead.add(ws)
    ws_clients -= dead


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_clients.discard(ws)


# ── REST API ───────────────────────────────────────────────────

@app.get("/api/health")
async def health():
    return {"status": "ok", "service": "quakeguard-cloud"}


@app.get("/api/events", response_model=list[SeismicEvent])
async def list_events(hub_id: Optional[str] = None, limit: int = 50):
    async with db_pool.acquire() as conn:
        if hub_id:
            rows = await conn.fetch("""
                SELECT * FROM events WHERE hub_id = $1
                ORDER BY timestamp DESC LIMIT $2
            """, hub_id, limit)
        else:
            rows = await conn.fetch("""
                SELECT * FROM events ORDER BY timestamp DESC LIMIT $1
            """, limit)
    return [SeismicEvent(
        id=r["id"], hub_id=r["hub_id"], event_id=r["event_id"],
        timestamp=r["timestamp"], severity=r["severity"],
        magnitude=r["magnitude_x10"] / 10.0,
        epicenter_dist_km=r["epicenter_dist_km"],
        actions_taken=r["actions_taken"], node_count=r["node_count"]
    ) for r in rows]


@app.get("/api/events/{event_id}")
async def get_event(event_id: int):
    async with db_pool.acquire() as conn:
        event = await conn.fetchrow(
            "SELECT * FROM events WHERE id = $1", event_id)
        if not event:
            raise HTTPException(404, "Event not found")
        waveforms = await conn.fetch(
            "SELECT * FROM waveforms WHERE event_id = $1", event_id)
        structural = await conn.fetch(
            "SELECT * FROM structural_reports WHERE event_id = $1", event_id)
        family = await conn.fetch(
            "SELECT * FROM family_responses WHERE event_id = $1", event_id)
    return {
        "event": dict(event),
        "waveforms": [dict(w) for w in waveforms],
        "structural_assessment": [dict(s) for s in structural],
        "family_responses": [dict(f) for f in family],
    }


@app.get("/api/nodes", response_model=list[NodeStatus])
async def list_nodes(hub_id: Optional[str] = None):
    async with db_pool.acquire() as conn:
        if hub_id:
            rows = await conn.fetch(
                "SELECT * FROM nodes WHERE hub_id = $1 ORDER BY last_seen DESC",
                hub_id)
        else:
            rows = await conn.fetch(
                "SELECT * FROM nodes ORDER BY last_seen DESC LIMIT 200")
    return [NodeStatus(
        hub_id=r["hub_id"], node_addr=r["node_addr"],
        node_type=r["node_type"], battery_pct=r["battery_pct"],
        temperature_c=r["temperature_c"], status=r["status"],
        last_seen=r["last_seen"]
    ) for r in rows]


@app.get("/api/structural")
async def structural_reports(hub_id: Optional[str] = None, limit: int = 100):
    async with db_pool.acquire() as conn:
        if hub_id:
            rows = await conn.fetch("""
                SELECT * FROM structural_reports WHERE hub_id = $1
                ORDER BY timestamp DESC LIMIT $2
            """, hub_id, limit)
        else:
            rows = await conn.fetch(
                "SELECT * FROM structural_reports ORDER BY timestamp DESC LIMIT $1",
                limit)
    return [dict(r) for r in rows]


@app.post("/api/family/response")
async def family_response(resp: FamilyResponse):
    async with db_pool.acquire() as conn:
        await conn.execute("""
            INSERT INTO family_responses (hub_id, event_id, user_id, status)
            VALUES ($1, $2, $3, $4)
        """, resp.hub_id, resp.event_id, resp.user_id, resp.status)
    await broadcast_ws({
        "type": "family_response",
        "hub_id": resp.hub_id,
        "event_id": resp.event_id,
        "user_id": resp.user_id,
        "status": resp.status,
    })
    return {"status": "recorded"}


@app.get("/api/reports/structural")
async def structural_report_pdf(hub_id: str):
    """Generate a civil-engineer-ready PDF structural health report."""
    # In production: use reportlab to generate full PDF
    # For now: return JSON summary
    async with db_pool.acquire() as conn:
        reports = await conn.fetch("""
            SELECT * FROM structural_reports WHERE hub_id = $1
            AND timestamp > NOW() - INTERVAL '30 days'
            ORDER BY timestamp ASC
        """, hub_id)
        events = await conn.fetch("""
            SELECT * FROM events WHERE hub_id = $1
            AND timestamp > NOW() - INTERVAL '30 days'
            ORDER BY timestamp ASC
        """, hub_id)

    return {
        "hub_id": hub_id,
        "report_period": "30 days",
        "total_events": len(events),
        "structural_samples": len(reports),
        "max_strain": max((r["strain_max_micro"] for r in reports), default=0),
        "max_anomaly": max((r["anomaly_score"] for r in reports), default=0),
        "avg_resonance_shift": (
            sum(r["resonance_shift_hz"] for r in reports) / len(reports)
            if reports else 0
        ),
        "recommendation": (
            "No structural anomalies detected. Next inspection per schedule."
            if max((r["anomaly_score"] for r in reports), default=0) < 128
            else "Anomalous strain detected. Recommend immediate structural inspection."
        ),
        "report_date": datetime.now(timezone.utc).isoformat(),
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
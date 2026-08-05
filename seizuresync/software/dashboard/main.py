"""
SeizureSync — FastAPI backend main application
SPDX-License-Identifier: MIT
"""
import os
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager

from routes.patients import router as patients_router
from routes.events import router as events_router
from routes.reports import router as reports_router
from routes.alerts import router as alerts_router
from mqtt_ingest import mqtt_loop
from inference import InferenceService
from twilio_dispatch import TwilioDispatcher
import models

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup/shutdown lifecycle."""
    # Start MQTT ingestion in background
    import asyncio
    mqtt_task = asyncio.create_task(mqtt_loop())
    app.state.mqtt_task = mqtt_task
    app.state.inference = InferenceService()
    app.state.twilio = TwilioDispatcher()
    yield
    mqtt_task.cancel()

app = FastAPI(
    title="SeizureSync API",
    version="1.0.0",
    description="AI-powered seizure detection & epilepsy management backend",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # production: restrict to mobile app origin
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(patients_router, prefix="/patients", tags=["patients"])
app.include_router(events_router, prefix="/patients/{patient_id}/events",
                    tags=["events"])
app.include_router(reports_router, prefix="/patients/{patient_id}/reports",
                   tags=["reports"])
app.include_router(alerts_router, prefix="/patients/{patient_id}/alerts",
                   tags=["alerts"])


@app.get("/health")
async def health():
    return {"status": "ok", "service": "seizuresync"}


@app.websocket("/ws/{patient_id}")
async def ws_alert_stream(ws: WebSocket, patient_id: str):
    """Real-time alert stream for mobile app."""
    await ws.accept()
    try:
        while True:
            # In production: push from MQTT/Redis pubsub
            data = await ws.receive_text()
            # Echo for now; real impl pushes alerts
            await ws.send_text(f'{{"patient":"{patient_id}","ack":true}}')
    except WebSocketDisconnect:
        pass
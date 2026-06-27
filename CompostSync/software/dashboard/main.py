"""
CompostSync Cloud Backend — FastAPI
software/dashboard/main.py
"""
import os
import logging
from datetime import datetime
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from routers import devices, telemetry, compost, alerts, auth
from mqtt.client import mqtt_client
from ml.inference import ml_inference

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("compostsync.dashboard")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events."""
    logger.info("Starting CompostSync backend...")
    await mqtt_client.connect()
    ml_inference.load_models()
    logger.info("Backend ready.")
    yield
    logger.info("Shutting down...")
    await mqtt_client.disconnect()


app = FastAPI(
    title="CompostSync API",
    description="AI-powered home composting intelligence system",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router, prefix="/api/auth", tags=["auth"])
app.include_router(devices.router, prefix="/api/devices", tags=["devices"])
app.include_router(telemetry.router, prefix="/api/telemetry", tags=["telemetry"])
app.include_router(compost.router, prefix="/api/compost", tags=["compost"])
app.include_router(alerts.router, prefix="/api/alerts", tags=["alerts"])


@app.get("/")
async def root():
    return {
        "service": "CompostSync API",
        "version": "1.0.0",
        "status": "running",
        "docs": "/docs",
    }


@app.get("/health")
async def health():
    return {"status": "ok", "mqtt_connected": mqtt_client.is_connected}
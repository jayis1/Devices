"""
PestSync Cloud Backend — FastAPI
software/dashboard/main.py
"""
from fastapi import FastAPI
from contextlib import asynccontextmanager
import uvicorn

from config import settings
from db import database
from mqtt.client import mqtt_client
from routers import devices, telemetry, detections, traps, deterrents, alerts, auth


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown lifecycle."""
    # Startup
    await database.connect()
    await mqtt_client.connect()
    print(f"PestSync backend started on port {settings.port}")

    yield

    # Shutdown
    await mqtt_client.disconnect()
    await database.disconnect()
    print("PestSync backend stopped")


app = FastAPI(
    title="PestSync API",
    description="AI-powered pest detection, identification & deterrence system",
    version="1.0.0",
    lifespan=lifespan,
)

# Register routers
app.include_router(auth.router, prefix="/api/auth", tags=["auth"])
app.include_router(devices.router, prefix="/api/devices", tags=["devices"])
app.include_router(telemetry.router, prefix="/api/telemetry", tags=["telemetry"])
app.include_router(detections.router, prefix="/api/detections", tags=["detections"])
app.include_router(traps.router, prefix="/api/traps", tags=["traps"])
app.include_router(deterrents.router, prefix="/api/deterrents", tags=["deterrents"])
app.include_router(alerts.router, prefix="/api/alerts", tags=["alerts"])


@app.get("/")
async def root():
    return {
        "system": "PestSync",
        "version": "1.0.0",
        "description": "AI-powered pest detection, identification & deterrence",
        "docs": "/docs",
    }


@app.get("/health")
async def health():
    return {"status": "ok", "mqtt": mqtt_client.connected, "db": database.is_connected}


if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=settings.port, reload=True)
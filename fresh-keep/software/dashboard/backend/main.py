"""
FreshKeep — Cloud Dashboard Backend (FastAPI + MQTT)

Real-time kitchen intelligence: inventory tracking, spoilage prediction,
fire safety alerts, shopping list generation, recipe suggestions.

MQTT topics:
  freshkeep/fridge/data      — Fridge sensor data (from hub)
  freshkeep/pantry/data      — Pantry sensor data
  freshkeep/stove/data       — Stove guard data
  freshkeep/fire/alarm       — Fire alarm events
  freshkeep/inventory/update — Inventory changes
  freshkeep/commands/{node}  — Commands to nodes (via hub)
"""

from fastapi import FastAPI, WebSocket, HTTPException, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime, timedelta
import asyncio
import json
import logging
from contextlib import asynccontextmanager

import sqlalchemy as sa
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession, async_sessionmaker
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column
import aiomqtt

# ── Configuration ──────────────────────────────────────────────────────────
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_PREFIX = "freshkeep"
DATABASE_URL = "postgresql+asyncpg://freshkeep:freshkeep@localhost/freshkeep"

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("freshkeep")

# ── Database Models ───────────────────────────────────────────────────────
class Base(DeclarativeBase):
    pass

class FridgeReading(Base):
    __tablename__ = "fridge_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    voc_index: Mapped[int] = mapped_column(sa.Integer)
    co2_ppm: Mapped[int] = mapped_column(sa.Integer)
    ethylene_raw: Mapped[int] = mapped_column(sa.Integer)
    temp_c_x10: Mapped[int] = mapped_column(sa.Integer)
    humidity_x10: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf1: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf2: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf3: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf4: Mapped[int] = mapped_column(sa.Integer)
    door_state: Mapped[int] = mapped_column(sa.SmallInteger)
    spoilage_score: Mapped[int] = mapped_column(sa.SmallInteger)
    image_ready: Mapped[int] = mapped_column(sa.SmallInteger)
    battery_pct: Mapped[int] = mapped_column(sa.SmallInteger)

class PantryReading(Base):
    __tablename__ = "pantry_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    temp_c_x10: Mapped[int] = mapped_column(sa.Integer)
    humidity_x10: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf1: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf2: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf3: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf4: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf5: Mapped[int] = mapped_column(sa.Integer)
    weight_mg_shelf6: Mapped[int] = mapped_column(sa.Integer)
    door_state: Mapped[int] = mapped_column(sa.SmallInteger)
    barcode_ready: Mapped[int] = mapped_column(sa.SmallInteger)
    image_ready: Mapped[int] = mapped_column(sa.SmallInteger)
    items_count: Mapped[int] = mapped_column(sa.SmallInteger)
    battery_pct: Mapped[int] = mapped_column(sa.SmallInteger)

class StoveReading(Base):
    __tablename__ = "stove_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    max_temp_c: Mapped[int] = mapped_column(sa.Integer)
    avg_temp_c: Mapped[int] = mapped_column(sa.Integer)
    lpg_ppm: Mapped[int] = mapped_column(sa.Integer)
    co_ppm: Mapped[int] = mapped_column(sa.Integer)
    nh3_ppm: Mapped[int] = mapped_column(sa.Integer)
    smoke_level: Mapped[int] = mapped_column(sa.SmallInteger)
    flame_detected: Mapped[int] = mapped_column(sa.SmallInteger)
    burner_state: Mapped[int] = mapped_column(sa.SmallInteger)
    motion_detected: Mapped[int] = mapped_column(sa.SmallInteger)
    gas_valve_state: Mapped[int] = mapped_column(sa.SmallInteger)
    fire_confidence: Mapped[int] = mapped_column(sa.SmallInteger)
    alert_level: Mapped[int] = mapped_column(sa.SmallInteger)

class InventoryItem(Base):
    __tablename__ = "inventory_items"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    action: Mapped[str] = mapped_column(sa.String(20))  # added, removed, expired, consumed
    location: Mapped[str] = mapped_column(sa.String(20))  # fridge, pantry
    barcode: Mapped[Optional[str]] = mapped_column(sa.String(32), nullable=True)
    name: Mapped[str] = mapped_column(sa.String(128))
    weight_mg: Mapped[int] = mapped_column(sa.Integer)
    expiry_days: Mapped[Optional[int]] = mapped_column(sa.Integer, nullable=True)
    category: Mapped[str] = mapped_column(sa.String(32))
    expiry_date: Mapped[Optional[datetime]] = mapped_column(sa.DateTime, nullable=True)
    still_fresh: Mapped[bool] = mapped_column(sa.Boolean, default=True)

class FireAlarm(Base):
    __tablename__ = "fire_alarms"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    max_temp_c: Mapped[int] = mapped_column(sa.Integer)
    lpg_ppm: Mapped[int] = mapped_column(sa.Integer)
    smoke_level: Mapped[int] = mapped_column(sa.SmallInteger)
    flame_detected: Mapped[int] = mapped_column(sa.SmallInteger)
    fire_confidence: Mapped[int] = mapped_column(sa.SmallInteger)
    source_node: Mapped[int] = mapped_column(sa.SmallInteger)
    resolved: Mapped[bool] = mapped_column(sa.Boolean, default=False)
    resolution: Mapped[Optional[str]] = mapped_column(sa.String(100), nullable=True)

class ShoppingItem(Base):
    __tablename__ = "shopping_items"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    name: Mapped[str] = mapped_column(sa.String(128))
    category: Mapped[str] = mapped_column(sa.String(32))
    quantity: Mapped[str] = mapped_column(sa.String(32))
    priority: Mapped[str] = mapped_column(sa.String(20))  # low, medium, high, urgent
    source: Mapped[str] = mapped_column(sa.String(32))  # auto, manual, suggestion
    purchased: Mapped[bool] = mapped_column(sa.Boolean, default=False)


# ── Pydantic Schemas ───────────────────────────────────────────────────────
class FridgeData(Base):
    voc_index: int
    co2_ppm: int
    ethylene_raw: int
    temp_c_x10: int
    humidity_x10: int
    weight_mg: List[int] = Field(max_length=4)
    door_state: int
    spoilage_score: int
    image_ready: int
    battery_pct: int

class PantryData(Base):
    temp_c_x10: int
    humidity_x10: int
    weight_mg: List[int] = Field(max_length=6)
    door_state: int
    barcode_ready: int
    image_ready: int
    items_count: int
    battery_pct: int

class StoveData(Base):
    max_temp_c: int
    avg_temp_c: int
    lpg_ppm: int
    co_ppm: int
    nh3_ppm: int
    smoke_level: int
    flame_detected: int
    burner_state: int
    motion_detected: int
    gas_valve_state: int
    fire_confidence: int
    alert_level: int

class InventoryUpdate(Base):
    action: str
    location: str
    barcode: Optional[str] = None
    name: str
    weight_mg: int
    expiry_days: Optional[int] = None
    category: str

class ShoppingItemCreate(Base):
    name: str
    category: str
    quantity: str = "1"
    priority: str = "medium"
    source: str = "manual"


# ── Application Setup ──────────────────────────────────────────────────────
engine = create_async_engine(DATABASE_URL, echo=False)
async_session = async_sessionmaker(engine, expire_on_commit=False)

ws_connections: List[WebSocket] = []

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: create tables, connect MQTT
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    
    # Start MQTT subscriber in background
    asyncio.create_task(mqtt_subscriber())
    logger.info("FreshKeep backend started")
    yield
    
    # Shutdown
    await engine.dispose()
    logger.info("FreshKeep backend stopped")

app = FastAPI(
    title="FreshKeep API",
    description="AI-powered kitchen intelligence system",
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


# ── MQTT Subscriber ────────────────────────────────────────────────────────
async def mqtt_subscriber():
    """Subscribe to all FreshKeep MQTT topics and persist data."""
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.subscribe(f"{MQTT_PREFIX}/#")
        logger.info(f"Subscribed to {MQTT_PREFIX}/#")
        
        async for message in client.messages:
            try:
                topic = str(message.topic)
                payload = json.loads(message.payload.decode())
                
                if topic == f"{MQTT_PREFIX}/fridge/data":
                    await handle_fridge_data(payload)
                elif topic == f"{MQTT_PREFIX}/pantry/data":
                    await handle_pantry_data(payload)
                elif topic == f"{MQTT_PREFIX}/stove/data":
                    await handle_stove_data(payload)
                elif topic == f"{MQTT_PREFIX}/fire/alarm":
                    await handle_fire_alarm(payload)
                elif topic == f"{MQTT_PREFIX}/inventory/update":
                    await handle_inventory_update(payload)
                
                # Broadcast to WebSocket clients
                for ws in ws_connections:
                    try:
                        await ws.send_json({"topic": topic, "data": payload})
                    except Exception:
                        ws_connections.remove(ws)
                        
            except Exception as e:
                logger.error(f"MQTT message error: {e}")


async def handle_fridge_data(data: dict):
    """Persist fridge sensor data and check for alerts."""
    async with async_session() as session:
        reading = FridgeReading(
            voc_index=data.get("voc_index", 0),
            co2_ppm=data.get("co2_ppm", 400),
            ethylene_raw=data.get("ethylene_raw", 0),
            temp_c_x10=data.get("temp_c_x10", 40),
            humidity_x10=data.get("humidity_x10", 500),
            weight_mg_shelf1=data.get("weight_mg", [0])[0] if len(data.get("weight_mg", [])) > 0 else 0,
            weight_mg_shelf2=data.get("weight_mg", [0])[1] if len(data.get("weight_mg", [])) > 1 else 0,
            weight_mg_shelf3=data.get("weight_mg", [0])[2] if len(data.get("weight_mg", [])) > 2 else 0,
            weight_mg_shelf4=data.get("weight_mg", [0])[3] if len(data.get("weight_mg", [])) > 3 else 0,
            door_state=data.get("door_state", 0),
            spoilage_score=data.get("spoilage_score", 0),
            image_ready=data.get("image_ready", 0),
            battery_pct=data.get("battery_pct", 100),
        )
        session.add(reading)
        await session.commit()
    
    # Check for spoilage alerts
    if data.get("spoilage_score", 0) > 80:
        logger.warning(f"FRIDGE SPOILAGE ALERT: score={data['spoilage_score']}")


async def handle_pantry_data(data: dict):
    """Persist pantry sensor data and check for low stock."""
    async with async_session() as session:
        reading = PantryReading(
            temp_c_x10=data.get("temp_c_x10", 220),
            humidity_x10=data.get("humidity_x10", 450),
            weight_mg_shelf1=data.get("weight_mg", [0])[0] if len(data.get("weight_mg", [])) > 0 else 0,
            weight_mg_shelf2=data.get("weight_mg", [0])[1] if len(data.get("weight_mg", [])) > 1 else 0,
            weight_mg_shelf3=data.get("weight_mg", [0])[2] if len(data.get("weight_mg", [])) > 2 else 0,
            weight_mg_shelf4=data.get("weight_mg", [0])[3] if len(data.get("weight_mg", [])) > 3 else 0,
            weight_mg_shelf5=data.get("weight_mg", [0])[4] if len(data.get("weight_mg", [])) > 4 else 0,
            weight_mg_shelf6=data.get("weight_mg", [0])[5] if len(data.get("weight_mg", [])) > 5 else 0,
            door_state=data.get("door_state", 0),
            barcode_ready=data.get("barcode_ready", 0),
            image_ready=data.get("image_ready", 0),
            items_count=data.get("items_count", 0),
            battery_pct=data.get("battery_pct", 100),
        )
        session.add(reading)
        await session.commit()


async def handle_stove_data(data: dict):
    """Persist stove guard data and check for fire safety alerts."""
    async with async_session() as session:
        reading = StoveReading(
            max_temp_c=data.get("max_temp_c", 25),
            avg_temp_c=data.get("avg_temp_c", 25),
            lpg_ppm=data.get("lpg_ppm", 0),
            co_ppm=data.get("co_ppm", 0),
            nh3_ppm=data.get("nh3_ppm", 0),
            smoke_level=data.get("smoke_level", 0),
            flame_detected=data.get("flame_detected", 0),
            burner_state=data.get("burner_state", 0),
            motion_detected=data.get("motion_detected", 0),
            gas_valve_state=data.get("gas_valve_state", 0),
            fire_confidence=data.get("fire_confidence", 0),
            alert_level=data.get("alert_level", 0),
        )
        session.add(reading)
        await session.commit()
    
    # Critical fire alert
    if data.get("alert_level", 0) >= 4:  # EMERGENCY
        logger.critical(f"FIRE EMERGENCY: confidence={data.get('fire_confidence')}, "
                       f"max_temp={data.get('max_temp_c')}°C, lpg={data.get('lpg_ppm')}ppm")


async def handle_fire_alarm(data: dict):
    """Log fire alarm event."""
    async with async_session() as session:
        alarm = FireAlarm(
            max_temp_c=data.get("max_temp_c", 0),
            lpg_ppm=data.get("lpg_ppm", 0),
            smoke_level=data.get("smoke_level", 0),
            flame_detected=data.get("flame_detected", 0),
            fire_confidence=data.get("fire_confidence", 0),
            source_node=data.get("source_node", 3),
        )
        session.add(alarm)
        await session.commit()
    
    logger.critical(f"FIRE ALARM LOGGED: temp={data.get('max_temp_c')}°C, "
                    f"confidence={data.get('fire_confidence')}%")


async def handle_inventory_update(data: dict):
    """Process barcode scan or manual inventory change."""
    async with async_session() as session:
        # Calculate expiry date
        expiry_days = data.get("expiry_days")
        expiry_date = None
        if expiry_days:
            expiry_date = datetime.utcnow() + timedelta(days=expiry_days)
        
        item = InventoryItem(
            action=data.get("action", "added"),
            location=data.get("location", "pantry"),
            barcode=data.get("barcode"),
            name=data.get("name", "Unknown"),
            weight_mg=data.get("weight_mg", 0),
            expiry_days=expiry_days,
            category=data.get("category", "other"),
            expiry_date=expiry_date,
            still_fresh=True,
        )
        session.add(item)
        await session.commit()
    
    # Auto-generate shopping list if item removed or expired
    if data.get("action") in ("removed", "expired"):
        await generate_shopping_suggestion(data)


async def generate_shopping_suggestion(data: dict):
    """Suggest a replacement for a consumed or expired item."""
    async with async_session() as session:
        suggestion = ShoppingItem(
            name=data.get("name", "Unknown"),
            category=data.get("category", "other"),
            quantity="1",
            priority="medium" if data.get("action") == "removed" else "high",
            source="auto",
            purchased=False,
        )
        session.add(suggestion)
        await session.commit()


# ── REST API Endpoints ────────────────────────────────────────────────────

@app.get("/api/status")
async def get_system_status():
    """Get overall system status."""
    async with async_session() as session:
        # Latest readings
        fridge = (await session.execute(
            sa.select(FridgeReading).order_by(FridgeReading.timestamp.desc()).limit(1)
        )).scalar_one_or_none()
        
        pantry = (await session.execute(
            sa.select(PantryReading).order_by(PantryReading.timestamp.desc()).limit(1)
        )).scalar_one_or_none()
        
        stove = (await session.execute(
            sa.select(StoveReading).order_by(StoveReading.timestamp.desc()).limit(1)
        )).scalar_one_or_none()
        
        fire_alerts = (await session.execute(
            sa.select(FireAlarm).where(FireAlarm.resolved == False).limit(5)
        )).scalars().all()
    
    return {
        "status": "online",
        "fridge": {
            "spoilage_score": fridge.spoilage_score if fridge else 0,
            "temp_c": (fridge.temp_c_x10 / 10.0) if fridge else 0,
            "door": fridge.door_state if fridge else 0,
            "battery_pct": fridge.battery_pct if fridge else 100,
        } if fridge else None,
        "pantry": {
            "items_count": pantry.items_count if pantry else 0,
            "temp_c": (pantry.temp_c_x10 / 10.0) if pantry else 0,
            "door": pantry.door_state if pantry else 0,
        } if pantry else None,
        "stove_guard": {
            "alert_level": stove.alert_level if stove else 0,
            "max_temp_c": stove.max_temp_c if stove else 25,
            "gas_valve_open": stove.gas_valve_state if stove else 0,
            "fire_confidence": stove.fire_confidence if stove else 0,
        } if stove else None,
        "fire_alerts": len(fire_alerts),
    }


@app.get("/api/fridge/history")
async def get_fridge_history(hours: int = Query(default=24, ge=1, le=168)):
    """Get fridge sensor history for the last N hours."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        readings = (await session.execute(
            sa.select(FridgeReading)
            .where(FridgeReading.timestamp >= since)
            .order_by(FridgeReading.timestamp)
        )).scalars().all()
    
    return [{
        "timestamp": r.timestamp.isoformat(),
        "voc_index": r.voc_index,
        "co2_ppm": r.co2_ppm,
        "ethylene_raw": r.ethylene_raw,
        "temp_c": r.temp_c_x10 / 10.0,
        "humidity_pct": r.humidity_x10 / 10.0,
        "spoilage_score": r.spoilage_score,
        "door_state": r.door_state,
    } for r in readings]


@app.get("/api/stove/history")
async def get_stove_history(hours: int = Query(default=24, ge=1, le=168)):
    """Get stove guard history for the last N hours."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        readings = (await session.execute(
            sa.select(StoveReading)
            .where(StoveReading.timestamp >= since)
            .order_by(StoveReading.timestamp)
        )).scalars().all()
    
    return [{
        "timestamp": r.timestamp.isoformat(),
        "max_temp_c": r.max_temp_c,
        "avg_temp_c": r.avg_temp_c,
        "lpg_ppm": r.lpg_ppm,
        "co_ppm": r.co_ppm,
        "smoke_level": r.smoke_level,
        "flame_detected": r.flame_detected,
        "burner_state": r.burner_state,
        "fire_confidence": r.fire_confidence,
        "alert_level": r.alert_level,
    } for r in readings]


@app.get("/api/inventory")
async def get_inventory(location: Optional[str] = None, fresh_only: bool = True):
    """Get current inventory items."""
    async with async_session() as session:
        query = sa.select(InventoryItem)
        if location:
            query = query.where(InventoryItem.location == location)
        if fresh_only:
            query = query.where(InventoryItem.still_fresh == True)
        query = query.order_by(InventoryItem.expiry_date.asc().nulls_last())
        
        items = (await session.execute(query)).scalars().all()
    
    return [{
        "id": i.id,
        "name": i.name,
        "location": i.location,
        "barcode": i.barcode,
        "category": i.category,
        "weight_mg": i.weight_mg,
        "expiry_date": i.expiry_date.isoformat() if i.expiry_date else None,
        "days_until_expiry": (i.expiry_date - datetime.utcnow()).days if i.expiry_date else None,
        "still_fresh": i.still_fresh,
    } for i in items]


@app.post("/api/inventory")
async def add_inventory_item(item: InventoryUpdate):
    """Add or update an inventory item (manual entry or barcode scan)."""
    async with async_session() as session:
        expiry_date = None
        if item.expiry_days:
            expiry_date = datetime.utcnow() + timedelta(days=item.expiry_days)
        
        db_item = InventoryItem(
            action=item.action,
            location=item.location,
            barcode=item.barcode,
            name=item.name,
            weight_mg=item.weight_mg,
            expiry_days=item.expiry_days,
            category=item.category,
            expiry_date=expiry_date,
            still_fresh=True,
        )
        session.add(db_item)
        await session.commit()
        await session.refresh(db_item)
    
    return {"id": db_item.id, "status": "added"}


@app.get("/api/shopping-list")
async def get_shopping_list():
    """Get auto-generated shopping list."""
    async with async_session() as session:
        items = (await session.execute(
            sa.select(ShoppingItem)
            .where(ShoppingItem.purchased == False)
            .order_by(
                sa.case({"urgent": 0, "high": 1, "medium": 2, "low": 3},
                        value=ShoppingItem.priority)
            )
        )).scalars().all()
    
    return [{
        "id": i.id,
        "name": i.name,
        "category": i.category,
        "quantity": i.quantity,
        "priority": i.priority,
        "source": i.source,
    } for i in items]


@app.post("/api/shopping-list")
async def add_shopping_item(item: ShoppingItemCreate):
    """Add item to shopping list."""
    async with async_session() as session:
        db_item = ShoppingItem(
            name=item.name,
            category=item.category,
            quantity=item.quantity,
            priority=item.priority,
            source=item.source,
            purchased=False,
        )
        session.add(db_item)
        await session.commit()
        await session.refresh(db_item)
    
    return {"id": db_item.id, "status": "added"}


@app.put("/api/shopping-list/{item_id}/purchased")
async def mark_item_purchased(item_id: int):
    """Mark a shopping list item as purchased."""
    async with async_session() as session:
        item = (await session.execute(
            sa.select(ShoppingItem).where(ShoppingItem.id == item_id)
        )).scalar_one_or_none()
        
        if not item:
            raise HTTPException(status_code=404, detail="Item not found")
        
        item.purchased = True
        await session.commit()
    
    return {"status": "purchased"}


@app.get("/api/recipes/suggestions")
async def get_recipe_suggestions(max_results: int = Query(default=10, ge=1, le=50)):
    """Suggest recipes based on expiring items in inventory."""
    async with async_session() as session:
        # Find items expiring within 3 days
        soon = datetime.utcnow() + timedelta(days=3)
        expiring = (await session.execute(
            sa.select(InventoryItem)
            .where(InventoryItem.expiry_date <= soon)
            .where(InventoryItem.still_fresh == True)
            .order_by(InventoryItem.expiry_date.asc())
            .limit(10)
        )).scalars().all()
    
    # In production, this would call an LLM-based recipe engine
    # For now, return the expiring items as "use soon" suggestions
    suggestions = []
    for item in expiring:
        suggestions.append({
            "priority": "high",
            "item_name": item.name,
            "category": item.category,
            "days_until_expiry": (item.expiry_date - datetime.utcnow()).days if item.expiry_date else None,
            "suggestion": f"Use {item.name} soon — expires in "
                         f"{(item.expiry_date - datetime.utcnow()).days if item.expiry_date else '?'} days",
        })
    
    return {"suggestions": suggestions[:max_results]}


@app.get("/api/fire-alarms")
async def get_fire_alarms(resolved: Optional[bool] = None, limit: int = Query(default=20)):
    """Get fire alarm history."""
    async with async_session() as session:
        query = sa.select(FireAlarm).order_by(FireAlarm.timestamp.desc()).limit(limit)
        if resolved is not None:
            query = query.where(FireAlarm.resolved == resolved)
        
        alarms = (await session.execute(query)).scalars().all()
    
    return [{
        "id": a.id,
        "timestamp": a.timestamp.isoformat(),
        "max_temp_c": a.max_temp_c,
        "lpg_ppm": a.lpg_ppm,
        "smoke_level": a.smoke_level,
        "flame_detected": a.flame_detected,
        "fire_confidence": a.fire_confidence,
        "source_node": a.source_node,
        "resolved": a.resolved,
    } for a in alarms]


@app.post("/api/commands/{node}")
async def send_command(node: str, command: str, params: Optional[dict] = None):
    """Send command to a node via MQTT (through hub)."""
    valid_nodes = ["fridge", "pantry", "stove-guard"]
    valid_commands = ["gas_shutoff", "suppression_on", "suppression_off", 
                      "photo_trigger", "barcode_scan", "calibrate_weight",
                      "reset"]
    
    if node not in valid_nodes:
        raise HTTPException(status_code=400, detail=f"Invalid node: {node}")
    if command not in valid_commands:
        raise HTTPException(status_code=400, detail=f"Invalid command: {command}")
    
    # Publish command via MQTT
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        payload = json.dumps({"command": command, "params": params or {}})
        await client.publish(f"{MQTT_PREFIX}/commands/{node}", payload)
    
    return {"status": "sent", "node": node, "command": command}


# ── WebSocket for Real-time Updates ────────────────────────────────────────
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Real-time data streaming to dashboard."""
    await websocket.accept()
    ws_connections.append(websocket)
    try:
        while True:
            # Keep connection alive
            data = await websocket.receive_text()
            # Could handle incoming commands here
    except Exception:
        ws_connections.remove(websocket)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
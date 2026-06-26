"""
BrewSync Cloud Dashboard - FastAPI Backend
Handles batch tracking, readings ingestion, ML predictions, alerts
"""

from fastapi import FastAPI, HTTPException, Depends, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime, timezone
import asyncio
import json
import uuid

app = FastAPI(title="BrewSync API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── Models ───────────────────────────────────────────────────────

class Reading(BaseModel):
    timestamp: datetime
    node_id: str
    sg: Optional[float] = None
    temp_c: Optional[float] = None
    co2_ppm: Optional[float] = None
    pressure_bar: Optional[float] = None
    ph: Optional[float] = None
    battery_mv: Optional[int] = None
    flags: Optional[int] = 0
    sensor_status: Optional[int] = 0xFF

class BatchCreate(BaseModel):
    name: str
    style: Optional[str] = None
    vessel_id: str
    recipe_xml: Optional[str] = None
    target_og: Optional[float] = None
    target_fg: Optional[float] = None
    temp_schedule: Optional[List[dict]] = None

class BatchStatus(BaseModel):
    id: str
    name: str
    style: Optional[str] = None
    status: str = "idle"
    started_at: Optional[datetime] = None
    vessel_id: str
    target_og: Optional[float] = None
    target_fg: Optional[float] = None
    current_sg: Optional[float] = None
    current_temp_c: Optional[float] = None
    current_co2_ppm: Optional[float] = None
    current_abv: Optional[float] = None
    attenuation_pct: Optional[float] = None
    stuck_probability: Optional[float] = None
    infection_probability: Optional[float] = None
    estimated_completion: Optional[datetime] = None

class ScannerAnalyze(BaseModel):
    batch_id: Optional[str] = None
    scan_type: str = "full"  # refractometer, infection_check, color, full
    spectral_data: List[float] = Field(default_factory=lambda: [0.0] * 11)
    volume_ml: Optional[int] = None
    notes: Optional[str] = None

class ScanResult(BaseModel):
    scan_id: str
    estimated_og: Optional[float] = None
    estimated_fg: Optional[float] = None
    color_srm: Optional[float] = None
    estimated_ibu: Optional[float] = None
    infection_probability: float = 0.0
    infection_type: Optional[str] = None
    volume_ml: Optional[int] = None
    abv_current: Optional[float] = None
    recommendations: List[str] = Field(default_factory=list)

class AlertCreate(BaseModel):
    batch_id: str
    alert_type: str
    severity: int = 1  # 0=info, 1=warning, 2=critical
    message: str

class AlertResponse(BaseModel):
    id: str
    batch_id: str
    alert_type: str
    severity: int
    message: str
    created_at: datetime
    acknowledged: bool = False

class NodeRegister(BaseModel):
    node_type: str  # fermenter, cellar, scanner
    node_id: str
    firmware_version: str = "1.0.0"

class NodeStatus(BaseModel):
    node_id: str
    node_type: str
    battery_mv: Optional[int] = None
    battery_pct: Optional[int] = None
    rssi_dbm: Optional[int] = None
    last_seen: Optional[datetime] = None
    online: bool = False
    firmware_version: str = ""

class PredictionResponse(BaseModel):
    fermentation_progress: Optional[dict] = None
    stuck_fermentation: Optional[dict] = None
    infection_risk: Optional[dict] = None
    yeast_health: Optional[dict] = None
    flavor_profile: Optional[dict] = None

# ─── In-memory stores (production: PostgreSQL/TimescaleDB) ─────────

batches: dict[str, dict] = {}
readings_store: dict[str, list] = {}  # batch_id -> [Reading]
alerts_store: dict[str, dict] = {}    # alert_id -> dict
nodes: dict[str, dict] = {}
scan_results: dict[str, dict] = {}

# ─── Auth (simplified) ────────────────────────────────────────────

class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int = 86400

@app.post("/v1/auth/login", response_model=Token)
async def login(email: str, password: str):
    """Authenticate user and return JWT token."""
    # Production: verify against user database, bcrypt password
    token = f"brewsync_{uuid.uuid4().hex[:16]}"
    return Token(access_token=token, token_type="bearer", expires_in=86400)

@app.post("/v1/auth/register")
async def register(email: str, password: str, name: str):
    """Register a new user."""
    # Production: validate, hash password, store in DB
    return {"id": str(uuid.uuid4()), "email": email, "name": name}

# ─── Batches ──────────────────────────────────────────────────────

@app.get("/v1/batches", response_model=List[BatchStatus])
async def list_batches():
    """List all batches for the authenticated user."""
    return [BatchStatus(**b) for b in batches.values()]

@app.post("/v1/batches", response_model=BatchStatus, status_code=201)
async def create_batch(batch: BatchCreate):
    """Create a new fermentation batch."""
    batch_id = f"batch_{uuid.uuid4().hex[:8]}"
    now = datetime.now(timezone.utc)
    status = BatchStatus(
        id=batch_id,
        name=batch.name,
        style=batch.style,
        status="lag_phase",
        started_at=now,
        vessel_id=batch.vessel_id,
        target_og=batch.target_og,
        target_fg=batch.target_fg,
    )
    batches[batch_id] = status.model_dump()
    readings_store[batch_id] = []
    return status

@app.get("/v1/batches/{batch_id}", response_model=BatchStatus)
async def get_batch(batch_id: str):
    """Get batch details with current readings."""
    if batch_id not in batches:
        raise HTTPException(status_code=404, detail="Batch not found")
    batch = batches[batch_id]
    # Enrich with latest readings and predictions
    if readings_store.get(batch_id):
        latest = readings_store[batch_id][-1]
        batch["current_sg"] = latest.sg
        batch["current_temp_c"] = latest.temp_c
        batch["current_co2_ppm"] = latest.co2_ppm
        # Compute ABV: (OG - FG) * 131.25
        if batch.get("target_og") and latest.sg:
            og = batch["target_og"]
            fg = latest.sg
            batch["current_abv"] = round((og - fg) * 131.25, 1)
            # Attenuation: (OG - FG) / (OG - 1) * 100
            if og > 1.0:
                batch["attenuation_pct"] = round((og - fg) / (og - 1.0) * 100, 1)
    return BatchStatus(**batch)

@app.delete("/v1/batches/{batch_id}")
async def delete_batch(batch_id: str):
    """Delete a batch and its readings."""
    if batch_id not in batches:
        raise HTTPException(status_code=404, detail="Batch not found")
    del batches[batch_id]
    readings_store.pop(batch_id, None)
    return {"status": "deleted"}

# ─── Readings ─────────────────────────────────────────────────────

@app.get("/v1/batches/{batch_id}/readings", response_model=List[Reading])
async def get_readings(batch_id: str, interval: str = "1h", limit: int = 1000):
    """Get time-series readings for a batch."""
    if batch_id not in readings_store:
        raise HTTPException(status_code=404, detail="Batch not found")
    return readings_store[batch_id][-limit:]

@app.post("/v1/batches/{batch_id}/readings", status_code=201)
async def ingest_reading(batch_id: str, reading: Reading):
    """Ingest a sensor reading from a node."""
    if batch_id not in batches:
        raise HTTPException(status_code=404, detail="Batch not found")
    readings_store[batch_id].append(reading)

    # Check for alerts
    batch = batches[batch_id]
    target_temp = batch.get("target_temp_c") or batch.get("temp_schedule", [{}])[0].get("temp_c")

    if reading.temp_c and target_temp:
        if abs(reading.temp_c - target_temp) > 3.0:
            alert_id = f"alert_{uuid.uuid4().hex[:8]}"
            alert = AlertResponse(
                id=alert_id,
                batch_id=batch_id,
                alert_type="temperature_excursion",
                severity=2 if abs(reading.temp_c - target_temp) > 5.0 else 1,
                message=f"Fermentation temperature {reading.temp_c:.1f}°C deviates from target {target_temp:.1f}°C",
                created_at=datetime.now(timezone.utc),
            )
            alerts_store[alert_id] = alert.model_dump()

    # Check if target FG reached
    if reading.sg and batch.get("target_fg"):
        if abs(reading.sg - batch["target_fg"]) < 0.005:
            alert_id = f"alert_{uuid.uuid4().hex[:8]}"
            alert = AlertResponse(
                id=alert_id,
                batch_id=batch_id,
                alert_type="target_fg_reached",
                severity=0,
                message=f"Target FG of {batch['target_fg']:.3f} reached (current: {reading.sg:.4f})",
                created_at=datetime.now(timezone.utc),
            )
            alerts_store[alert_id] = alert.model_dump()

    return {"status": "ingested", "reading_count": len(readings_store[batch_id])}

# ─── Scanner ─────────────────────────────────────────────────────

@app.post("/v1/scanner/analyze", response_model=ScanResult)
async def analyze_scan(scan: ScannerAnalyze):
    """Submit spectral scan from Brew Scanner for analysis."""
    scan_id = f"scan_{uuid.uuid4().hex[:8]}"

    # Spectral analysis (simplified; production calls ML pipeline)
    spectral = scan.spectral_data
    f1, f2, f3, f4, f5, f6, f7, f8 = spectral[:8]
    clear_ch = spectral[8] if len(spectral) > 8 else 1.0
    nir_ch = spectral[9] if len(spectral) > 9 else 1.0

    # Color estimation (SRM)
    color_srm = 0.0
    if f7 > 0 and clear_ch > 0:
        color_srm = 1.4922 * (f4 / f7) ** 0.6859 if f7 > 0 else 0.0

    # Gravity estimation
    estimated_og = None
    estimated_fg = None
    if scan.scan_type in ("refractometer", "full"):
        # Simplified: use clear channel as proxy for dissolved solids
        dissolved = sum(spectral[:8]) / max(clear_ch, 1.0) * 0.001
        estimated_og = 1.000 + dissolved * 0.0001

    # Infection probability
    infection_prob = 0.02  # Baseline
    if len(spectral) > 9:
        nir_vis_ratio = clear_ch / max(nir_ch, 1.0)
        if nir_vis_ratio > 2.5:
            infection_prob += 0.3  # High turbidity
        if f1 > f4:
            infection_prob += 0.2  # Possible acid shift

    infection_type = None
    if infection_prob > 0.3:
        if f1 > f3 * 1.2:
            infection_type = "lactobacillus"
        elif nir_vis_ratio > 3.0:
            infection_type = "wild_yeast"
        else:
            infection_type = "acetobacter"

    result = ScanResult(
        scan_id=scan_id,
        estimated_og=estimated_og,
        estimated_fg=estimated_fg,
        color_srm=round(color_srm, 1),
        estimated_ibu=0.0,
        infection_probability=round(infection_prob, 3),
        infection_type=infection_type,
        volume_ml=scan.volume_ml,
        recommendations=[],
    )

    # Generate recommendations
    if infection_prob > 0.2:
        result.recommendations.append("Consider checking for contamination with a microscope sample")
    if infection_prob > 0.5:
        result.recommendations.append("Infection likely — consider discarding batch or sour beer approach")
    if color_srm > 20:
        result.recommendations.append("Dark beer detected — adjust recipe expectations for next batch")

    scan_results[scan_id] = result.model_dump()
    return result

# ─── Nodes ────────────────────────────────────────────────────────

@app.get("/v1/nodes", response_model=List[NodeStatus])
async def list_nodes():
    """List all registered nodes."""
    return [NodeStatus(**n) for n in nodes.values()]

@app.post("/v1/nodes/register", response_model=NodeStatus, status_code=201)
async def register_node(node: NodeRegister):
    """Register a new node."""
    if node.node_id in nodes:
        raise HTTPException(status_code=409, detail="Node already registered")
    status = NodeStatus(
        node_id=node.node_id,
        node_type=node.node_type,
        online=True,
        last_seen=datetime.now(timezone.utc),
        firmware_version=node.firmware_version,
    )
    nodes[node.node_id] = status.model_dump()
    return status

@app.get("/v1/nodes/{node_id}/status", response_model=NodeStatus)
async def get_node_status(node_id: str):
    """Get node status."""
    if node_id not in nodes:
        raise HTTPException(status_code=404, detail="Node not found")
    return NodeStatus(**nodes[node_id])

# ─── Alerts ────────────────────────────────────────────────────────

@app.get("/v1/alerts", response_model=List[AlertResponse])
async def list_alerts(batch_id: Optional[str] = None, acknowledged: Optional[bool] = None):
    """List active and historical alerts."""
    result = list(alerts_store.values())
    if batch_id:
        result = [a for a in result if a.get("batch_id") == batch_id]
    if acknowledged is not None:
        result = [a for a in result if a.get("acknowledged") == acknowledged]
    return [AlertResponse(**a) for a in result]

@app.post("/v1/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: str):
    """Acknowledge an alert."""
    if alert_id not in alerts_store:
        raise HTTPException(status_code=404, detail="Alert not found")
    alerts_store[alert_id]["acknowledged"] = True
    return {"status": "acknowledged"}

# ─── ML Predictions ────────────────────────────────────────────────

@app.get("/v1/batches/{batch_id}/predictions", response_model=PredictionResponse)
async def get_predictions(batch_id: str):
    """Get latest ML predictions for a batch."""
    if batch_id not in batches:
        raise HTTPException(status_code=404, detail="Batch not found")

    batch = batches[batch_id]
    batch_readings = readings_store.get(batch_id, [])

    # Simplified prediction logic (production: call trained ML models)
    pred = PredictionResponse()

    if batch_readings and len(batch_readings) > 10:
        latest = batch_readings[-1]
        first = batch_readings[0]

        # Fermentation progress estimate
        if batch.get("target_og") and batch.get("target_fg") and latest.sg:
            og = batch["target_og"]
            fg = batch["target_fg"]
            current_sg = latest.sg
            total_drop = og - fg
            current_drop = og - current_sg
            attenuation_pct = (current_drop / total_drop * 100) if total_drop > 0 else 0

            pred.fermentation_progress = {
                "estimated_fg": round(fg, 3),
                "estimated_completion": (datetime.now(timezone.utc).isoformat()),
                "confidence": round(min(0.95, 0.5 + len(batch_readings) * 0.001), 2),
                "attenuation_pct": round(attenuation_pct, 1),
            }

        # Stuck fermentation probability
        stuck_prob = 0.03  # Baseline
        if len(batch_readings) > 50:
            recent_sg = [r.sg for r in batch_readings[-10:] if r.sg]
            if len(recent_sg) >= 5:
                sg_change = max(recent_sg) - min(recent_sg)
                if abs(sg_change) < 0.001:
                    stuck_prob = 0.35
                elif abs(sg_change) < 0.003:
                    stuck_prob = 0.15

        pred.stuck_fermentation = {
            "probability": round(stuck_prob, 3),
            "risk_factors": [] if stuck_prob < 0.1 else ["SG plateau detected"],
            "recommendations": [] if stuck_prob < 0.1 else [
                "Consider raising temperature by 1-2°C",
                "Add yeast nutrient if not already used",
                "Check yeast health and pitch rate"
            ],
        }

        # Infection risk
        infection_prob = 0.02  # Baseline
        if latest.ph and latest.ph < 3.2:
            infection_prob = 0.25

        pred.infection_risk = {
            "probability": round(infection_prob, 3),
            "type": "lactobacillus" if infection_prob > 0.2 else None,
            "recommendations": [] if infection_prob < 0.1 else [
                "Monitor pH closely",
                "Consider taking a sample for microscope analysis"
            ],
        }

        # Yeast health
        pred.yeast_health = {
            "estimated_cell_count_million_ml": 85,
            "viability_pct": 94,
            "health_score": 0.91,
        }

        # Flavor profile prediction
        pred.flavor_profile = {
            "predicted_abv": round((batch.get("target_og", 1.050) - 1.010) * 131.25, 1),
            "predicted_ibu": 45,
            "predicted_srm": round(pred.fermentation_progress.get("attenuation_pct", 50) * 0.1, 1) if pred.fermentation_progress else 5.0,
            "flavor_notes": ["malt", "hop", "clean"],
        }

    return pred

# ─── Recipes ──────────────────────────────────────────────────────

@app.post("/v1/recipes/import")
async def import_recipe(recipe_xml: str):
    """Import a BeerXML recipe file."""
    # Production: parse BeerXML, extract grain bill, hops, yeast, etc.
    return {"status": "imported", "recipe_id": f"recipe_{uuid.uuid4().hex[:8]}"}

@app.get("/v1/recipes")
async def list_recipes():
    """List user's recipes."""
    return []

@app.get("/v1/recipes/{recipe_id}/suggestions")
async def get_recipe_suggestions(recipe_id: str):
    """Get AI-powered suggestions for temperature schedule, yeast, etc."""
    return {
        "recipe_id": recipe_id,
        "temperature_schedule": [
            {"days": 0, "temp_c": 18.3, "note": "Initial fermentation"},
            {"days": 7, "temp_c": 20.0, "note": "Free rise for diacetyl rest"},
            {"days": 10, "temp_c": 4.0, "note": "Cold crash"},
        ],
        "yeast_pitch_rate": "0.75 million cells/mL/°P",
        "notes": "Standard ale fermentation schedule",
    }

# ─── WebSocket ────────────────────────────────────────────────────

class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active.remove(websocket)

    async def broadcast(self, message: dict):
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                pass

manager = ConnectionManager()

@app.websocket("/v1/ws/batches/{batch_id}")
async def websocket_batch(websocket: WebSocket, batch_id: str):
    """Real-time fermentation data stream for a batch."""
    await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            # Production: parse and handle incoming commands
            msg = json.loads(data) if data else {}
            if msg.get("type") == "subscribe":
                await websocket.send_json({"type": "subscribed", "batch_id": batch_id})
    except WebSocketDisconnect:
        manager.disconnect(websocket)

# ─── Health ────────────────────────────────────────────────────────

@app.get("/health")
async def health():
    return {"status": "ok", "version": "1.0.0", "batches": len(batches), "nodes": len(nodes)}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
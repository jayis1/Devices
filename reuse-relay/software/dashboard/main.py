from datetime import datetime, timedelta, timezone
from uuid import uuid4
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="ReuseRelay local API", version="1")
offers = {}
CATEGORIES = {"furniture", "kitchenware", "clothing", "books", "other"}
class NewOffer(BaseModel):
    category: str
    expires_in_hours: int = Field(default=24, ge=1, le=48)
@app.get('/health')
def health(): return {"status": "ok"}
@app.get('/offers')
def list_offers():
    now = datetime.now(timezone.utc)
    for key in list(offers):
        if offers[key]["expires_at"] <= now: del offers[key]
    return list(offers.values())
@app.post('/offers')
def create_offer(body: NewOffer):
    if body.category not in CATEGORIES: raise HTTPException(422, "unsupported category")
    oid = str(uuid4())
    offers[oid] = {"id": oid, "category": body.category, "status": "available", "expires_at": datetime.now(timezone.utc)+timedelta(hours=body.expires_in_hours)}
    return offers[oid]
@app.post('/offers/{offer_id}/picked-up')
def picked_up(offer_id: str):
    if offer_id not in offers: raise HTTPException(404, "offer not found")
    offers[offer_id]["status"] = "picked_up"
    return offers[offer_id]

from fastapi import FastAPI, HTTPException, Request

app = FastAPI(title="HomeReady local prototype")
_events: dict[str, dict] = {}

@app.post("/events", status_code=202)
async def ingest(request: Request):
    raw = await request.body()
    if not raw or len(raw) > 384:
        raise HTTPException(413, "event must be 1..384 bytes")
    try:
        event = await request.json()
    except Exception as exc:
        raise HTTPException(400, "invalid JSON") from exc
    if not isinstance(event, dict) or event.get("version") != 1 or not isinstance(event.get("event_id"), str):
        raise HTTPException(422, "invalid event envelope")
    duplicate = event["event_id"] in _events
    _events.setdefault(event["event_id"], event)
    return {"accepted": True, "duplicate": duplicate}

@app.get("/events")
def list_events():
    return list(_events.values())

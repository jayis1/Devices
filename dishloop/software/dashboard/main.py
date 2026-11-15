from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from time import time
import os
app = FastAPI(title="DishLoop local site API", version="0.1.0")
events = []
last_seq = {}
class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0)
    kind: str
    ts_ms: int
    value: float
@app.post('/events', status_code=201)
def ingest(event: Event):
    if event.kind not in {'rack_mass_g', 'return_passage'}:
        raise HTTPException(422, 'unsupported event kind')
    if event.ts_ms > int(time()*1000) or int(time()*1000)-event.ts_ms > 86400000:
        raise HTTPException(422, 'stale or future event')
    if event.seq <= last_seq.get(event.node, -1):
        raise HTTPException(409, 'duplicate or out-of-order sequence')
    last_seq[event.node] = event.seq
    events.append(event.model_dump())
    return {'accepted': True}
@app.get('/summary')
def summary():
    return {'events': len(events), 'latest': events[-1] if events else None, 'mode': 'local prototype; staff verification required'}
@app.post('/staff-actions')
def staff_action(action: dict):
    if action.get('action') not in {'wash_started', 'wash_completed', 'rack_reset'}:
        raise HTTPException(422, 'unknown staff action')
    events.append({'kind':'staff_action', **action})
    return {'recorded': True}

def make_mqtt_client():
    """Optional TLS publisher; configure only with site-scoped broker credentials."""
    import paho.mqtt.client as mqtt
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.tls_set()
    username, password = os.getenv('DISHLOOP_MQTT_USER'), os.getenv('DISHLOOP_MQTT_PASSWORD')
    if username and password:
        client.username_pw_set(username, password)
    return client

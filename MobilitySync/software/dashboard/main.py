from __future__ import annotations

from fastapi import FastAPI

from ml_inference import assess_doorway, assess_fatigue, assess_transfer, assess_walker
from models import BandTelemetryIn, CommandOut, DoorwayTelemetryIn, TransferTelemetryIn, WalkerTelemetryIn

app = FastAPI(title='MobilitySync Dashboard', version='0.1.0')

STATE = {
    'walker': {},
    'transfer': {},
    'doorway': {},
    'band': {},
    'alerts': [],
    'commands': [],
}

@app.get('/health')
def health() -> dict:
    return {'status': 'ok', 'service': 'mobilitysync-dashboard'}

@app.post('/telemetry/walker')
def ingest_walker(payload: WalkerTelemetryIn) -> dict:
    assessment = assess_walker(payload.slip_score, payload.grip_force_n, payload.wheel_speed_rps)
    record = payload.model_dump() | assessment
    STATE['walker'][payload.walker_id] = record
    if assessment['action'] == 'pulse_brake':
        cmd = CommandOut(target=payload.walker_id, action='pulse_brake', reason='runaway_risk').model_dump()
        STATE['commands'].append(cmd)
        STATE['alerts'].append({'type': 'walker', 'walker_id': payload.walker_id, 'risk': assessment['runaway_risk']})
    return record

@app.post('/telemetry/transfer')
def ingest_transfer(payload: TransferTelemetryIn) -> dict:
    assessment = assess_transfer(payload.asymmetry_pct, payload.unload_rate, payload.retries)
    record = payload.model_dump() | assessment
    STATE['transfer'][payload.zone_id] = record
    if assessment['level'] != 'normal':
        cmd = CommandOut(target=payload.zone_id, action=assessment['cue'], reason='transfer_risk').model_dump()
        STATE['commands'].append(cmd)
        STATE['alerts'].append({'type': 'transfer', 'zone_id': payload.zone_id, 'level': assessment['level']})
    return record

@app.post('/telemetry/doorway')
def ingest_doorway(payload: DoorwayTelemetryIn) -> dict:
    assessment = assess_doorway(payload.range_m, payload.obstruction)
    record = payload.model_dump() | assessment
    STATE['doorway'][payload.doorway_id] = record
    if assessment['recommended_action'] != 'idle':
        STATE['commands'].append(CommandOut(target=payload.doorway_id, action=assessment['recommended_action'], reason='approach').model_dump())
    return record

@app.post('/telemetry/band')
def ingest_band(payload: BandTelemetryIn) -> dict:
    fatigue = assess_fatigue(payload.hr_bpm, payload.hrv_proxy, len(STATE['transfer']), len([a for a in STATE['alerts'] if a['type'] == 'walker']))
    record = payload.model_dump() | fatigue
    STATE['band'][payload.wearer_id] = record
    if payload.sos or payload.fall_confidence >= 80:
        STATE['alerts'].append({'type': 'band', 'wearer_id': payload.wearer_id, 'sos': payload.sos, 'fall_confidence': payload.fall_confidence})
    return record

@app.get('/commands')
def commands() -> list[dict]:
    return STATE['commands']

@app.get('/summary')
def summary() -> dict:
    high_fatigue = sum(1 for band in STATE['band'].values() if band['fatigue_band'] == 'high')
    unsafe_transfers = sum(1 for tr in STATE['transfer'].values() if tr['level'] != 'normal')
    walkway_events = len([a for a in STATE['alerts'] if a['type'] == 'walker'])
    independence_score = max(0, 100 - unsafe_transfers * 18 - walkway_events * 10 - high_fatigue * 12)
    return {
        'independence_score': independence_score,
        'unsafe_transfers': unsafe_transfers,
        'walker_alerts': walkway_events,
        'high_fatigue_users': high_fatigue,
        'command_count': len(STATE['commands']),
    }

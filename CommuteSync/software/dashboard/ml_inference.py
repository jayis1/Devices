from __future__ import annotations


def _level(score: float) -> str:
    if score >= 0.75:
        return 'high'
    if score >= 0.4:
        return 'medium'
    return 'low'


def readiness_risk(entry_rows: list[dict], desk_rows: list[dict]) -> dict:
    if not entry_rows:
        return {'score': 0.0, 'level': 'low', 'reason': 'No doorway data yet.'}
    latest = entry_rows[0]
    missing_fraction = 1.0
    if latest['required_items']:
        missing_fraction = max(0.0, 1.0 - (latest['confirmed_items'] / latest['required_items']))
    urgency = min(1.0, latest['departure_in_minutes'] / 30.0)
    carryover = 0.15 if desk_rows and desk_rows[0]['items_left_behind'] > 0 else 0.0
    score = min(1.0, missing_fraction * 0.7 + (1.0 - urgency) * 0.2 + carryover)
    reason = 'Essentials missing before departure.' if missing_fraction > 0 else 'Departure checklist complete.'
    return {'score': round(score, 3), 'level': _level(score), 'reason': reason}


def lateness_risk(entry_rows: list[dict], mobility_rows: list[dict]) -> dict:
    if not mobility_rows:
        return {'score': 0.1, 'level': 'low', 'reason': 'No live route telemetry yet.'}
    latest = mobility_rows[0]
    eta_factor = min(1.0, max(0.0, latest['eta_delta_minutes'] / 15.0))
    duration_factor = min(1.0, latest['route_minutes'] / 90.0)
    departure_pressure = 0.0
    if entry_rows:
        departure_pressure = 1.0 if entry_rows[0]['departure_in_minutes'] <= 5 else 0.3
    score = min(1.0, eta_factor * 0.6 + duration_factor * 0.2 + departure_pressure * 0.2)
    reason = 'Transfer or traffic delay likely to affect arrival.' if score >= 0.4 else 'Route timing is stable.'
    return {'score': round(score, 3), 'level': _level(score), 'reason': reason}


def exposure_risk(mobility_rows: list[dict]) -> dict:
    if not mobility_rows:
        return {'score': 0.0, 'level': 'low', 'reason': 'No exposure telemetry yet.'}
    latest = mobility_rows[0]
    pm = min(1.0, latest['pm25_ug_m3'] / 55.0)
    voc = min(1.0, latest['voc_index'] / 300.0)
    vibration = min(1.0, latest['vibration_rms'] / 1.6)
    score = min(1.0, pm * 0.5 + voc * 0.2 + vibration * 0.3)
    reason = 'Current route is carrying elevated air or vibration burden.' if score >= 0.4 else 'Exposure burden is acceptable.'
    return {'score': round(score, 3), 'level': _level(score), 'reason': reason}


def theft_risk(bag_rows: list[dict], mobility_rows: list[dict]) -> dict:
    if not bag_rows:
        return {'score': 0.05, 'level': 'low', 'reason': 'No bag events yet.'}
    latest = bag_rows[0]
    tamper = latest['tamper_score']
    separation = min(1.0, latest['separation_m'] / 8.0)
    parked_bonus = 0.15 if mobility_rows and mobility_rows[0]['route_minutes'] == 0 else 0.0
    score = min(1.0, tamper * 0.65 + separation * 0.25 + parked_bonus)
    reason = 'Bag or parked vehicle movement looks suspicious.' if score >= 0.4 else 'Security state looks normal.'
    return {'score': round(score, 3), 'level': _level(score), 'reason': reason}


def recommended_actions(readiness: dict, lateness: dict, exposure: dict, theft: dict, entry_rows: list[dict]) -> list[str]:
    actions: list[str] = []
    if entry_rows:
        latest = entry_rows[0]
        if not latest['bag_present']:
            actions.append('Bag missing from departure set.')
        if not latest['badge_seen']:
            actions.append('Tap badge or wallet before leaving.')
        if not latest['keys_seen']:
            actions.append('Pick up keys from tray.')
    if lateness['score'] >= 0.4:
        actions.append('Leave earlier or accept lower-delay alternate route.')
    if exposure['score'] >= 0.4:
        actions.append('Switch to lower-PM route or close vents on high-traffic segment.')
    if theft['score'] >= 0.4:
        actions.append('Check bag or bike lock immediately.')
    return actions or ['No action needed.']

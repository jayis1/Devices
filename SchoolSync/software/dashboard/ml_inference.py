from __future__ import annotations


def readiness_score(payload: dict) -> dict:
    score = 100
    score -= max(payload.get('minutes_to_departure', 0) < 10, 0) * 15
    score -= 20 if not payload.get('backpack_present', False) else 0
    score -= 18 if not payload.get('lunch_packed', False) else 0
    score -= int(payload.get('weather_complexity', 0.0) * 8)
    score -= payload.get('interventions', 0) * 4
    score = max(0, min(100, score))
    late_5 = min(0.98, max(0.02, (100 - score) / 100))
    return {
        'readiness_score': score,
        'late_5m_prob': round(late_5, 3),
        'late_10m_prob': round(min(0.99, late_5 + 0.09), 3),
        'late_20m_prob': round(min(0.995, late_5 + 0.18), 3),
    }


def lunch_safety(payload: dict) -> dict:
    safe = payload.get('minutes_until_lunch', 0) + (60 if payload.get('ice_pack_present', False) else -30)
    safe += max(0, 800 - payload.get('mass_grams', 0)) // 20
    safe -= max(0, int(payload.get('ambient_temp_c', 22.0) - 24.0) * 8)
    if safe >= payload.get('minutes_until_lunch', 0):
        return {'risk_level': 'low', 'safe_until_minutes': safe, 'recommended_action': 'packed lunch is within safe window'}
    if payload.get('ice_pack_present', False):
        return {'risk_level': 'medium', 'safe_until_minutes': max(0, safe), 'recommended_action': 'add a second ice pack or move lunch to fridge'}
    return {'risk_level': 'high', 'safe_until_minutes': max(0, safe), 'recommended_action': 'replace with cold-safe meal and add ice pack'}


def route_anomaly(payload: dict) -> dict:
    score = abs(payload.get('route_minutes', 0) - payload.get('expected_minutes', 0)) / max(1, payload.get('expected_minutes', 1))
    if not payload.get('boarded', False):
        score += 0.45
    if payload.get('child_present', False) != payload.get('backpack_present', False):
        score += 0.35
    escalation = 'none'
    if score > 0.75:
        escalation = 'call_caregiver'
    elif score > 0.4:
        escalation = 'push_alert'
    return {'anomaly_score': round(min(score, 1.5), 3), 'escalation': escalation}


def overview(children: list[dict]) -> dict:
    return {
        'children': children,
        'alerts': [c for c in children if c['status'] != 'ready'],
        'household_score': round(sum(c['readiness_score'] for c in children) / max(1, len(children))),
    }

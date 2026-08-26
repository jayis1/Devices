from __future__ import annotations

def assess_transfer(asymmetry_pct: float, unload_rate: float, retries: int) -> dict:
    score = 100.0 - asymmetry_pct * 1.2 - max(0.0, 12.0 - unload_rate) * 2.0 - retries * 10.0
    if score >= 80:
        level = 'normal'
        cue = 'continue'
    elif score >= 60:
        level = 'amber'
        cue = 'pause_and_recenter'
    else:
        level = 'red'
        cue = 'request_assistance'
    return {'score': max(0.0, round(score, 1)), 'level': level, 'cue': cue}

def assess_walker(slip_score: float, grip_force_n: float, wheel_speed_rps: float) -> dict:
    runaway_risk = min(100.0, slip_score * 45.0 + max(0.0, wheel_speed_rps * 12.0 - grip_force_n * 0.08))
    action = 'pulse_brake' if runaway_risk >= 35 else 'release'
    return {'runaway_risk': round(runaway_risk, 1), 'action': action}

def assess_fatigue(hr_bpm: int, hrv_proxy: int, transfer_count: int, slip_events: int) -> dict:
    risk = min(100.0, (hr_bpm - 60) * 0.7 + max(0, 30 - hrv_proxy) * 1.4 + transfer_count * 2.2 + slip_events * 6.0)
    band = 'high' if risk >= 65 else 'moderate' if risk >= 40 else 'low'
    return {'fatigue_risk': round(risk, 1), 'fatigue_band': band}

def assess_doorway(range_m: float, obstruction: bool) -> dict:
    if obstruction:
        return {'recommended_action': 'hold_open', 'priority': 'safety'}
    if range_m < 1.8:
        return {'recommended_action': 'open_soft', 'priority': 'assist'}
    return {'recommended_action': 'idle', 'priority': 'normal'}

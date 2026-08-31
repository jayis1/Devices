from __future__ import annotations

from typing import Iterable


PROTEIN_SCALE = {'negative': 0, 'trace': 10, '1+': 25, '2+': 50, '3+': 80}
GLUCOSE_SCALE = {'negative': 0, 'trace': 8, '1+': 18, '2+': 35, '3+': 60}
KETONE_SCALE = {'negative': 0, 'trace': 6, 'small': 14, 'moderate': 32, 'large': 55}


def _status(score: int) -> str:
    if score >= 75:
        return 'high'
    if score >= 45:
        return 'medium'
    return 'low'


def reduced_movement_risk(band_rows: Iterable[dict]) -> dict:
    rows = list(band_rows)
    latest = rows[0]
    baseline = sum(item['movement_count_10m'] for item in rows) / max(1, len(rows))
    deficit = max(0.0, baseline - latest['movement_count_10m'])
    score = min(100, int(deficit * 8 + latest['posture_pct_supine'] * 0.5 + latest['ehg_activity_index'] * 0.8))
    return {
        'score': score,
        'status': _status(score),
        'rationale': f"movement baseline {baseline:.1f}/10m vs latest {latest['movement_count_10m']} with supine {latest['posture_pct_supine']}%",
    }


def hypertensive_risk(cuff_rows: Iterable[dict], strip_rows: Iterable[dict], pad_rows: Iterable[dict]) -> dict:
    cuff = list(cuff_rows)[0]
    strip = list(strip_rows)[0]
    pad = list(pad_rows)[0]
    score = 0
    score += max(0, cuff['systolic_mmHg'] - 120)
    score += max(0, cuff['diastolic_mmHg'] - 80) * 2
    score += int((1.0 - cuff['waveform_quality']) * 10)
    score += PROTEIN_SCALE[strip['protein_level']]
    score += int(max(0.0, strip['specific_gravity'] - 1.020) * 1000)
    score += min(20, pad['supine_minutes'] // 15)
    score = min(100, score)
    return {
        'score': score,
        'status': _status(score),
        'rationale': f"BP {cuff['systolic_mmHg']}/{cuff['diastolic_mmHg']} mmHg with protein {strip['protein_level']} and {pad['supine_minutes']} supine minutes",
    }


def supine_sleep_risk(pad_rows: Iterable[dict]) -> dict:
    pad = list(pad_rows)[0]
    score = min(100, int((pad['supine_minutes'] / max(pad['hours_recorded'], 1)) * 8 + pad['restlessness_index'] * 12))
    return {
        'score': score,
        'status': _status(score),
        'rationale': f"supine {pad['supine_minutes']} min, left-side {pad['left_side_minutes']} min, restlessness {pad['restlessness_index']:.2f}",
    }


def hydration_status(strip_rows: Iterable[dict]) -> str:
    strip = list(strip_rows)[0]
    dehydration = max(0, int((strip['specific_gravity'] - 1.015) * 1000)) + KETONE_SCALE[strip['ketone_level']]
    if strip['hydration_bottle_ml'] >= 2000 and dehydration < 15:
        return 'well hydrated'
    if dehydration >= 35:
        return 'dehydration risk'
    return 'needs more fluids'


def derive_alerts(movement: dict, pressure: dict, sleep: dict, hydration: str) -> list[str]:
    alerts: list[str] = []
    if movement['score'] >= 70:
        alerts.append('Repeat a guided fetal movement check now and contact care team if movement remains reduced.')
    if pressure['score'] >= 75:
        alerts.append('Hypertensive risk is high. Repeat BP after rest and escalate if symptoms are present.')
    if sleep['score'] >= 60:
        alerts.append('Late-pregnancy supine sleep burden is elevated; enable gentle overnight reposition prompts.')
    if hydration == 'dehydration risk':
        alerts.append('Hydration trend is concerning; increase fluids and repeat strip if advised by clinician.')
    return alerts


def recommended_actions(movement: dict, pressure: dict, sleep: dict, hydration: str) -> list[str]:
    actions = ['Complete symptom check-in for headache, swelling, vision changes, bleeding, pain, and nausea.']
    if movement['score'] >= 45:
        actions.append('Hydrate, lie on left side, and rerun a 10-minute movement session.')
    if pressure['score'] >= 45:
        actions.append('Take a repeat BP reading in 15 minutes using seated supported posture.')
    if hydration != 'well hydrated':
        actions.append('Log fluid intake target and enable bottle reminders for the next 4 hours.')
    if sleep['score'] >= 45:
        actions.append('Review pillow support and late-evening comfort positioning plan.')
    return actions

from __future__ import annotations

from typing import Any, Dict, Iterable

from .models import TelemetryEvent


def summarize_risk(events: Iterable[TelemetryEvent]) -> Dict[str, Any]:
    rows = list(events)
    if not rows:
        return {
            'risk_level': 'green',
            'score': 0.0,
            'heat_risk': 0.0,
            'harness_risk': 0.0,
            'handoff_risk': 0.0,
        }

    latest = rows[-1]
    heat_risk = max(0.0, (latest.cabin_temp_c - 25.0) * 4.2 + latest.heat_slope_c_per_min * 90.0 + latest.cry_score * 10.0)
    harness_risk = max(0.0, (26.0 - latest.strap_tension_n) * 3.2 + abs(latest.chest_clip_ratio - 0.58) * 120.0 + (0.0 if latest.buckle_closed else 45.0))
    handoff_risk = max(0.0, (0.0 if latest.caregiver_nearby else 35.0) + (0.0 if latest.handoff_complete else 25.0) + (0.0 if latest.bag_present else 10.0) + (20.0 if (latest.child_present and not latest.ignition_on) else 0.0))
    score = min(100.0, heat_risk * 0.45 + harness_risk * 0.30 + handoff_risk * 0.35)

    if score >= 70.0 or (latest.child_present and not latest.caregiver_nearby and latest.cabin_temp_c >= 38.0):
        level = 'red'
    elif score >= 35.0 or harness_risk >= 30.0:
        level = 'amber'
    else:
        level = 'green'

    return {
        'risk_level': level,
        'score': round(score, 2),
        'heat_risk': round(min(100.0, heat_risk), 2),
        'harness_risk': round(min(100.0, harness_risk), 2),
        'handoff_risk': round(min(100.0, handoff_risk), 2),
    }


def recommend(child_id: str, events: Iterable[TelemetryEvent]) -> Dict[str, Any]:
    summary = summarize_risk(events)
    latest = list(events)[-1]
    actions = []
    if summary['harness_risk'] >= 25.0:
        actions.append('Re-seat the child and tighten the harness until strap slack is removed; verify chest clip is at armpit height.')
    if summary['heat_risk'] >= 35.0:
        actions.append('Return to the vehicle now, remove the child immediately, and ventilate the cabin.')
    if summary['handoff_risk'] >= 25.0:
        actions.append('Complete the unload checklist: child, bag, medication, and destination confirmation.')
    if latest.child_present and not latest.ignition_on and not latest.caregiver_nearby:
        actions.append('Escalate to emergency contacts and trigger in-vehicle siren.')
    if not actions:
        actions.append('Trip state is normal; continue passive monitoring.')
    return {
        'child_id': child_id,
        'risk_level': summary['risk_level'],
        'summary': f"{child_id} is {summary['risk_level']} risk with heat={summary['heat_risk']}, harness={summary['harness_risk']}, handoff={summary['handoff_risk']}.",
        'actions': actions,
    }

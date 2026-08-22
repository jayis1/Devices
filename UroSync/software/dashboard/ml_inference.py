from __future__ import annotations

from statistics import mean


def hydration_risk(void_events: list[dict], bottle_events: list[dict]) -> float:
    if not void_events and not bottle_events:
        return 0.25
    sg = mean([event.get('sg_q1000', 1016) for event in void_events] or [1016])
    intake = mean([event.get('consumed_ml_day', 1600) for event in bottle_events] or [1600])
    risk = 0.35 + max(0.0, (sg - 1018) / 20.0) + max(0.0, (1700 - intake) / 2500.0)
    return round(min(0.99, max(0.01, risk)), 3)


def uti_risk(void_events: list[dict], nocturia_count: int) -> float:
    if not void_events:
        return round(min(0.8, 0.08 + nocturia_count * 0.04), 3)
    last = void_events[-1]
    score = 0.07
    score += 0.22 if last.get('leukocyte', 0) else 0.0
    score += 0.25 if last.get('nitrite', 0) else 0.0
    score += 0.08 if last.get('blood', 0) else 0.0
    score += max(0.0, (nocturia_count - 1) * 0.06)
    return round(min(0.99, score), 3)


def fall_risk(mat_events: list[dict], humidity_pct: float) -> float:
    if not mat_events:
        return round(min(0.9, 0.12 + max(0.0, humidity_pct - 60) / 200.0), 3)
    last = mat_events[-1]
    score = 0.08 + last.get('sway_index', 0) / 220.0 + last.get('transfer_latency_ms', 0) / 5000.0
    if last.get('slip_flag'):
        score += 0.2
    score += max(0.0, humidity_pct - 65.0) / 180.0
    return round(min(0.99, score), 3)


def nocturia_forecast(voids_per_night: list[int]) -> list[float]:
    base = mean(voids_per_night or [1.0])
    slope = ((voids_per_night[-1] - voids_per_night[0]) / max(1, len(voids_per_night) - 1)) if len(voids_per_night) > 1 else 0.0
    return [round(max(0.0, base + slope * day * 0.75), 2) for day in range(1, 8)]


def next_best_nudge(hydration: float, uti: float, fall: float) -> str:
    if fall > 0.6:
        return 'Enable guided amber night lighting and sit before standing again.'
    if uti > 0.55:
        return 'Repeat strip in 6 hours and escalate if pain, fever, or confusion are present.'
    if hydration > 0.5:
        return 'Drink 300 mL water now and distribute the next 900 mL over 6 hours.'
    return 'Maintain current routine and complete one morning sample tomorrow.'

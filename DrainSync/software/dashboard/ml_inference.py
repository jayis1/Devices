from __future__ import annotations

from typing import Iterable


def state_from_risk(risk: float) -> str:
    if risk >= 0.7:
        return 'red'
    if risk >= 0.35:
        return 'yellow'
    return 'green'


def backup_forecast(stack: dict, actuator: dict | None) -> dict:
    base = 0.12
    base += min(stack.get('level_mm', 0.0) / 1200.0, 0.35)
    base += min(max(stack.get('diff_pressure_pa', 0.0), 0.0) / 300.0, 0.25)
    base += min(stack.get('surge_count', 0) / 50.0, 0.20)
    if actuator and not actuator.get('healthy', True):
        base += 0.10
    risk = max(0.0, min(base, 0.99))
    return {'risk': round(risk, 3), 'state': state_from_risk(risk), 'hours_to_peak': max(1, int(48 - risk * 36))}


def clog_risks(flow_rows: Iterable[dict]) -> list[dict]:
    ranked = []
    for row in flow_rows:
        risk = 0.15
        risk += min(row.get('duration_ms', 0) / 4000.0, 0.35)
        risk += min(row.get('turbulence', 0.0) / 400.0, 0.25)
        risk += min(row.get('gas_index', 0.0) / 200.0, 0.10)
        if row.get('leak'):
            risk += 0.10
        ranked.append({'branch_id': row.get('branch_id', row.get('node_id', 'unknown')), 'risk': round(min(risk, 0.99), 3)})
    ranked.sort(key=lambda item: item['risk'], reverse=True)
    return ranked[:5]


def trap_recommendations(trap_rows: Iterable[dict]) -> list[dict]:
    items = []
    for row in trap_rows:
        action = 'none'
        volume = 0
        if row.get('trap_depth_raw', 999) < 280 and row.get('h2s_ppb', 0.0) > 20:
            action = 'prime'
            volume = 300
        elif row.get('h2s_ppb', 0.0) > 45:
            action = 'inspect'
        items.append({'node_id': row['node_id'], 'action': action, 'volume_ml': volume})
    return items


def valve_state(actuator: dict | None) -> dict:
    if not actuator:
        return {'position': 'open', 'healthy': True}
    pct = actuator.get('position_pct', 0.0)
    if pct >= 90:
        position = 'closed'
    elif pct <= 10:
        position = 'open'
    else:
        position = 'partial'
    return {'position': position, 'healthy': actuator.get('healthy', True)}

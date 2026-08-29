from __future__ import annotations

from statistics import mean


def outage_forecast(panel: dict) -> dict:
    stress = 0.0
    stress += max(0.0, 230.0 - panel.get('vrms', 230.0)) * 1.6
    stress += max(0.0, panel.get('thd_pct', 0.0) - 2.5) * 14.0
    stress += max(0.0, 60.0 - panel.get('freq_hz', 60.0)) * 180.0
    stress += (100.0 - panel.get('battery_soc', 100.0)) * 0.2
    expected = int(min(24 * 60, max(20, 35 + stress)))
    confidence = round(min(0.97, 0.45 + stress / 500.0), 3)
    strategy = 'battery_only' if expected < 120 else 'staged_generator_window'
    return {'expected_minutes': expected, 'confidence': confidence, 'strategy': strategy}


def load_decisions(outlets: list[dict], reserve_minutes: int) -> list[dict]:
    decisions = []
    for outlet in outlets:
        if outlet.get('medical'):
            decisions.append({'label': outlet['label'], 'action': 'keep_on', 'rationale': 'medical load protected'})
        elif reserve_minutes < 60 and outlet['priority'] >= 3:
            decisions.append({'label': outlet['label'], 'action': 'shed', 'rationale': 'reserve under 60 minutes'})
        elif reserve_minutes < 150 and outlet['priority'] >= 2 and outlet['watts'] > 120:
            decisions.append({'label': outlet['label'], 'action': 'cycle', 'rationale': 'moderate reserve, reduce average demand'})
        else:
            decisions.append({'label': outlet['label'], 'action': 'keep_on', 'rationale': 'within current power budget'})
    return decisions


def cold_summary(cold_nodes: list[dict]) -> str:
    if not cold_nodes:
        return 'No cold-chain nodes installed.'
    minimum = min(node['hold_minutes_remaining'] for node in cold_nodes)
    avg_temp = mean(node['product_temp_c'] for node in cold_nodes)
    return f'Minimum cold hold time is {minimum} minutes; mean product temperature is {avg_temp:.1f}°C.'

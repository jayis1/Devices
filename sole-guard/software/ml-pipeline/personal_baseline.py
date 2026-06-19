"""
SoleGuard Personal Baseline Learning

Learns a per-patient 14-day baseline for pressure distribution, temperature
symmetry, and gait pattern. Deviations from this personal baseline are far
more sensitive than population thresholds for early ulcer detection.

Outputs a baseline JSON consumed by the hub's alert engine.
"""

import os
import json
import numpy as np
from collections import defaultdict

BASELINE_DAYS = 14
OUTPUT_DIR    = os.path.join(os.path.dirname(__file__), "..", "baselines")


def compute_baseline(events: list[dict]) -> dict:
    """
    events: list of FootEvent dicts with pressure_l/r, temp_l/r, gait.
    Returns a baseline dict with per-zone mean + std.
    """
    zones = 6
    pressure_stats = {"L": defaultdict(list), "R": defaultdict(list)}
    temp_stats     = {"L": [], "R": []}
    gait_stats     = defaultdict(list)

    for ev in events:
        pL = ev.get("pressure_l", [])
        pR = ev.get("pressure_r", [])
        for z in range(zones):
            zL = pL[z*4:(z+1)*4] if pL else []
            zR = pR[z*4:(z+1)*4] if pR else []
            if zL: pressure_stats["L"][z].append(max(zL))
            if zR: pressure_stats["R"][z].append(max(zR))
        tL = ev.get("temp_l", [])
        tR = ev.get("temp_r", [])
        if tL: temp_stats["L"].append(tL)
        if tR: temp_stats["R"].append(tR)
        for i, v in enumerate(ev.get("gait", [])):
            gait_stats[i].append(v)

    baseline = {
        "pressure": {
            side: {str(z): {"mean": float(np.mean(vals)) if vals else 0,
                            "std":  float(np.std(vals))  if vals else 0}
                   for z, vals in zones_dict.items()}
            for side, zones_dict in pressure_stats.items()
        },
        "temp_asymmetry": {},
        "gait": {str(i): {"mean": float(np.mean(vals)) if vals else 0,
                          "std":  float(np.std(vals))  if vals else 0}
                 for i, vals in gait_stats.items()},
    }

    # Temperature asymmetry baseline
    asymmetries = defaultdict(list)
    for tL, tR in zip(temp_stats["L"], temp_stats["R"]):
        for i in range(min(len(tL), len(tR))):
            asymmetries[i].append(abs(tL[i] - tR[i]) / 100.0)
    baseline["temp_asymmetry"] = {
        str(i): {"mean": float(np.mean(vals)) if vals else 0,
                 "std":  float(np.std(vals))  if vals else 0}
        for i, vals in asymmetries.items()
    }
    return baseline


def save_baseline(patient_id: int, baseline: dict):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    path = os.path.join(OUTPUT_DIR, f"patient_{patient_id}_baseline.json")
    with open(path, "w") as f:
        json.dump(baseline, f, indent=2)
    print(f"Baseline saved to {path}")


if __name__ == "__main__":
    # Self-test with synthetic 14-day data
    rng = np.random.default_rng(42)
    synth = []
    for day in range(BASELINE_DAYS):
        for _ in range(120):  # 30s windows
            synth.append({
                "pressure_l": [int(rng.integers(0, 200)) for _ in range(24)],
                "pressure_r": [int(rng.integers(0, 200)) for _ in range(24)],
                "temp_l": [int(rng.normal(3200, 20)) for _ in range(8)],  # 32.00C
                "temp_r": [int(rng.normal(3200, 20)) for _ in range(8)],
                "gait": [int(rng.normal(1000, 100)) for _ in range(8)],
            })
    bl = compute_baseline(synth)
    save_baseline(0, bl)
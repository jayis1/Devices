"""
PawSync Personal Baseline Calculator

Computes per-pet baseline vital signs over the first 14 days of monitoring.
The baseline is used to detect deviations (HRV decline, HR elevation, temp
change) that indicate illness, pain, or stress.

Baseline metrics:
  - Resting HR (bpm) — averaged during sleep/rest periods
  - HRV-RMSSD (ms) — averaged during sleep/rest periods
  - Skin temperature (°C) — averaged during rest
  - Activity distribution — % time per activity class
  - Daily feeding intake (g) — average over 7 days

The baseline is established after 14 days (4032 samples at 5-min intervals)
and continuously refined with a sliding window thereafter.
"""

import os
import numpy as np
from datetime import datetime, timedelta
from typing import Optional

import torch
import torch.nn as nn

WINDOW_DAYS    = 14
SAMPLES_PER_DAY = 288  # 5-min intervals
BASELINE_SAMPLES = WINDOW_DAYS * SAMPLES_PER_DAY  # 4032
SLIDING_WINDOW   = 7   # days for ongoing baseline refinement


def compute_baseline(vitals_data: np.ndarray) -> dict:
    """
    Compute baseline from 14 days of vitals data.

    Args:
        vitals_data: array of shape (n_samples, 5) — [hr, hrv_ms, temp_c, activity, timestamp]

    Returns:
        dict with baseline metrics
    """
    # Filter to resting/sleeping periods (activity == 0 or 3)
    resting_mask = np.isin(vitals_data[:, 3], [0, 3])
    resting = vitals_data[resting_mask]

    if len(resting) < 100:
        return {"established": False, "reason": "insufficient resting data"}

    baseline = {
        "established": len(vitals_data) >= BASELINE_SAMPLES,
        "baseline_hr": float(np.median(resting[:, 0])),
        "baseline_hrv_ms": float(np.median(resting[:, 1])),
        "baseline_temp_c": float(np.median(resting[:, 2])),
        "hr_std": float(np.std(resting[:, 0])),
        "hrv_std": float(np.std(resting[:, 1])),
        "temp_std": float(np.std(resting[:, 2])),
        "sample_count": len(vitals_data),
        "resting_sample_count": len(resting),
    }

    # Compute normal ranges (mean ± 2σ)
    baseline["hr_range"] = (baseline["baseline_hr"] - 2 * baseline["hr_std"],
                           baseline["baseline_hr"] + 2 * baseline["hr_std"])
    baseline["hrv_range"] = (max(0, baseline["baseline_hrv_ms"] - 2 * baseline["hrv_std"]),
                             baseline["baseline_hrv_ms"] + 2 * baseline["hrv_std"])
    baseline["temp_range"] = (baseline["baseline_temp_c"] - 2 * baseline["temp_std"],
                              baseline["baseline_temp_c"] + 2 * baseline["temp_std"])

    return baseline


def compute_activity_distribution(activity_data: np.ndarray) -> dict:
    """
    Compute the per-pet daily activity distribution.

    Args:
        activity_data: array of activity classes (0-8), one per 5-min slot

    Returns:
        dict with % time per activity class
    """
    activity_names = ["resting", "walking", "running", "sleeping",
                      "scratching", "head_shaking", "licking", "eating", "playing"]
    total = len(activity_data)
    if total == 0:
        return {}

    distribution = {}
    for i, name in enumerate(activity_names):
        pct = np.sum(activity_data == i) / total * 100
        distribution[name] = float(pct)

    distribution["total_samples"] = total
    return distribution


def detect_deviation(current: dict, baseline: dict) -> list:
    """
    Detect deviations from baseline that may indicate illness.

    Returns list of deviation alerts.
    """
    alerts = []

    if not baseline.get("established"):
        return alerts

    # HRV decline > 20%
    if baseline["baseline_hrv_ms"] > 0:
        hrv_ratio = current["hrv_ms"] / baseline["baseline_hrv_ms"]
        if hrv_ratio < 0.80:
            decline_pct = (1 - hrv_ratio) * 100
            alerts.append({
                "type": "hrv_decline",
                "severity": "high",
                "message": f"HRV {decline_pct:.0f}% below baseline — possible pain or illness",
                "value": current["hrv_ms"],
                "baseline": baseline["baseline_hrv_ms"],
            })

    # HR elevation > 15%
    if baseline["baseline_hr"] > 0:
        hr_ratio = current["hr"] / baseline["baseline_hr"]
        if hr_ratio > 1.15:
            alerts.append({
                "type": "hr_elevated",
                "severity": "medium",
                "message": f"Resting HR elevated ({current['hr']:.0f} vs {baseline['baseline_hr']:.0f} bpm)",
                "value": current["hr"],
                "baseline": baseline["baseline_hr"],
            })

    # Temperature change > 0.5°C
    temp_diff = current["temp_c"] - baseline["baseline_temp_c"]
    if abs(temp_diff) > 0.5:
        direction = "elevated" if temp_diff > 0 else "depressed"
        severity = "high" if temp_diff > 0.8 else "medium"
        alerts.append({
            "type": "temp_abnormal",
            "severity": severity,
            "message": f"Skin temp {direction} by {abs(temp_diff):.1f}°C — {'possible fever' if temp_diff > 0 else 'check for hypothermia'}",
            "value": current["temp_c"],
            "baseline": baseline["baseline_temp_c"],
        })

    return alerts


def update_baseline_sliding(old_baseline: dict, new_data: np.ndarray,
                             window_days: int = SLIDING_WINDOW) -> dict:
    """
    Update baseline with sliding window for ongoing monitoring.

    Keeps the most recent `window_days` of data and recomputes.
    """
    resting_mask = np.isin(new_data[:, 3], [0, 3])
    resting = new_data[resting_mask]

    if len(resting) < 50:
        return old_baseline

    updated = {
        "established": True,
        "baseline_hr": float(np.median(resting[:, 0])),
        "baseline_hrv_ms": float(np.median(resting[:, 1])),
        "baseline_temp_c": float(np.median(resting[:, 2])),
        "hr_std": float(np.std(resting[:, 0])),
        "hrv_std": float(np.std(resting[:, 1])),
        "temp_std": float(np.std(resting[:, 2])),
        "sample_count": len(new_data),
        "resting_sample_count": len(resting),
        "window_days": window_days,
    }
    return updated


if __name__ == "__main__":
    # Demo: generate synthetic 14-day baseline
    rng = np.random.default_rng(42)
    n = BASELINE_SAMPLES
    data = np.zeros((n, 5))
    data[:, 0] = rng.normal(80, 10, n)      # HR
    data[:, 1] = rng.normal(40, 8, n)       # HRV ms
    data[:, 2] = rng.normal(38.5, 0.3, n)   # temp
    data[:, 3] = rng.choice([0, 0, 0, 1, 3, 3, 3], n)  # activity (weighted to rest)
    data[:, 4] = np.arange(n) * 300  # timestamps (5-min)

    baseline = compute_baseline(data)
    print("=== Baseline (14 days) ===")
    for k, v in baseline.items():
        print(f"  {k}: {v}")

    # Test deviation detection
    current = {"hr": 95, "hrv_ms": 28, "temp_c": 39.1}
    alerts = detect_deviation(current, baseline)
    print("\n=== Deviation Test (ill pet) ===")
    for a in alerts:
        print(f"  [{a['severity']}] {a['type']}: {a['message']}")
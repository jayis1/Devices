"""
CalmGrid Personal Baseline Calculator

Computes per-user baseline vital signs over the first 14 days of monitoring.
The baseline is used to detect deviations (HRV decline, EDA arousal, HR
elevation, prosody F0 shift) that indicate stress, allostatic load, or
approaching burnout.

Baseline metrics:
  - Resting HR (bpm) — averaged during sleep/rest periods
  - HRV-RMSSD (ms) — averaged during sleep/rest periods
  - EDA tonic SCL (µS) — averaged during rest
  - EDA SCR rate (events/min) — averaged during rest
  - Skin temperature (°C) — averaged during rest
  - Prosody F0 baseline (Hz) — averaged across all speech
  - Activity distribution — % time per activity class
  - Daily step count — average over 7 days
  - Sleep duration + efficiency — average over 7 days

The baseline is established after 14 days (4032 samples at 5-min intervals)
and continuously refined with a sliding window thereafter.
"""

import os
import numpy as np
from datetime import datetime, timedelta
from typing import Optional

WINDOW_DAYS    = 14
SAMPLES_PER_DAY = 288  # 5-min intervals
BASELINE_SAMPLES = WINDOW_DAYS * SAMPLES_PER_DAY  # 4032
SLIDING_WINDOW   = 7   # days for ongoing baseline refinement


def compute_baseline(vitals_data: np.ndarray) -> dict:
    """
    Compute baseline from 14 days of vitals data.

    Args:
        vitals_data: array of shape (n_samples, 7) —
                     [hr, hrv_ms, eda_scl, eda_scr, temp_c, activity, timestamp]

    Returns:
        dict with baseline metrics
    """
    # Filter to resting/sleeping periods (activity == 3 or 4)
    resting_mask = np.isin(vitals_data[:, 5], [3, 4])
    resting = vitals_data[resting_mask]

    if len(resting) < 100:
        return {"established": False, "reason": "insufficient resting data"}

    baseline = {
        "established": True,
        "baseline_hr": float(np.median(resting[:, 0])),
        "baseline_hrv_ms": float(np.median(resting[:, 1])),
        "baseline_eda_scl": float(np.median(resting[:, 2])),
        "baseline_eda_scr": float(np.median(resting[:, 3])),
        "baseline_temp_c": float(np.median(resting[:, 4])),
        "resting_pct": float(len(resting) / len(vitals_data)),
    }

    # Activity distribution (all data, not just resting)
    for cls in range(8):
        baseline[f"activity_{cls}_pct"] = float(
            np.mean(vitals_data[:, 5] == cls) * 100
        )

    # Step count average (if available)
    if vitals_data.shape[1] > 6:
        daily_steps = []
        for day in range(WINDOW_DAYS):
            day_start = day * SAMPLES_PER_DAY
            day_end = (day + 1) * SAMPLES_PER_DAY
            if day_end <= len(vitals_data):
                daily_steps.append(np.sum(vitals_data[day_start:day_end, 6]))
        if daily_steps:
            baseline["avg_daily_steps"] = float(np.mean(daily_steps))

    # Compute stress reactivity (how much HRV drops during stress vs rest)
    stress_mask = vitals_data[:, 5].isin([5, 6]) if hasattr(vitals_data[:, 5], 'isin') else \
                  np.isin(vitals_data[:, 5], [5, 6])
    stress_data = vitals_data[stress_mask]
    if len(stress_data) > 10:
        baseline["stress_hrv_ms"] = float(np.median(stress_data[:, 1]))
        baseline["hrv_reactivity"] = float(
            (baseline["baseline_hrv_ms"] - baseline["stress_hrv_ms"]) /
            baseline["baseline_hrv_ms"]
        )

    return baseline


def compute_prosody_baseline(prosody_data: np.ndarray) -> dict:
    """
    Compute prosody F0 baseline from speech data.

    Args:
        prosody_data: array of shape (n_samples, 3) — [f0, prosody_class, confidence]

    Returns:
        dict with prosody baseline
    """
    # Only use confident calm/neutral classifications
    mask = (prosody_data[:, 2] > 60) & (prosody_data[:, 1] <= 1)
    calm = prosody_data[mask]

    if len(calm) < 50:
        return {"established": False}

    return {
        "established": True,
        "baseline_f0": float(np.median(calm[:, 0])),
        "f0_variability": float(np.std(calm[:, 0])),
    }


def detect_deviation(current: dict, baseline: dict) -> dict:
    """
    Detect deviations from baseline that indicate stress.

    Returns dict with deviation flags + magnitudes.
    """
    deviations = {}

    if not baseline.get("established"):
        return deviations

    # HRV decline
    if baseline["baseline_hrv_ms"] > 0:
        hrv_ratio = current["hrv_ms"] / baseline["baseline_hrv_ms"]
        if hrv_ratio < 0.8:
            deviations["hrv_decline"] = {
                "severity": "medium" if hrv_ratio < 0.7 else "low",
                "magnitude": (1 - hrv_ratio) * 100,
            }

    # HR elevation
    if baseline["baseline_hr"] > 0:
        hr_ratio = current["hr"] / baseline["baseline_hr"]
        if hr_ratio > 1.1:
            deviations["hr_elevated"] = {
                "severity": "medium" if hr_ratio > 1.2 else "low",
                "magnitude": (hr_ratio - 1) * 100,
            }

    # EDA arousal
    if baseline.get("baseline_eda_scr", 0) > 0:
        scr_ratio = current["eda_scr"] / baseline["baseline_eda_scr"]
        if scr_ratio > 2.0:
            deviations["eda_arousal"] = {
                "severity": "high" if scr_ratio > 3 else "medium",
                "magnitude": (scr_ratio - 1) * 100,
            }

    # Temperature change
    temp_delta = current["temp_c"] - baseline["baseline_temp_c"]
    if abs(temp_delta) > 0.4:
        deviations["temp_change"] = {
            "severity": "low",
            "magnitude": abs(temp_delta),
        }

    return deviations


def refine_baseline(baseline: dict, new_data: np.ndarray) -> dict:
    """
    Refine baseline with a sliding 7-day window after initial establishment.
    Uses exponential moving average to gradually adapt to long-term changes.
    """
    alpha = 0.05  # slow adaptation
    resting_mask = np.isin(new_data[:, 5], [3, 4])
    resting = new_data[resting_mask]

    if len(resting) < 10:
        return baseline

    new_hr = np.median(resting[:, 0])
    new_hrv = np.median(resting[:, 1])

    baseline["baseline_hr"] = (1 - alpha) * baseline["baseline_hr"] + alpha * new_hr
    baseline["baseline_hrv_ms"] = (1 - alpha) * baseline["baseline_hrv_ms"] + alpha * new_hrv

    return baseline


if __name__ == "__main__":
    # Demo: generate synthetic 14-day data and compute baseline
    np.random.seed(42)
    n = BASELINE_SAMPLES
    data = np.zeros((n, 7))
    for i in range(n):
        hour = (i * 5 / 60) % 24
        if hour < 7 or hour > 22:  # sleeping
            data[i, 0] = 55 + np.random.normal(0, 3)   # HR
            data[i, 1] = 60 + np.random.normal(0, 5)   # HRV
            data[i, 5] = 4  # sleeping
        elif 9 < hour < 17:  # working
            data[i, 0] = 75 + np.random.normal(0, 5)
            data[i, 1] = 35 + np.random.normal(0, 5)
            data[i, 5] = 5  # working
        else:
            data[i, 0] = 70 + np.random.normal(0, 4)
            data[i, 1] = 45 + np.random.normal(0, 5)
            data[i, 5] = 1  # walking
        data[i, 2] = 5 + np.random.normal(0, 1)     # EDA SCL
        data[i, 3] = 2 + np.random.normal(0, 0.5)   # SCR rate
        data[i, 4] = 33 + np.random.normal(0, 0.3)  # temp
        data[i, 6] = np.random.randint(5000, 12000)  # steps

    baseline = compute_baseline(data)
    print("Baseline computed:")
    for k, v in baseline.items():
        print(f"  {k}: {v:.2f}" if isinstance(v, float) else f"  {k}: {v}")
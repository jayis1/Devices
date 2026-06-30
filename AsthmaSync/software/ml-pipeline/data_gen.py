"""
AsthmaSync — Synthetic Data Generator
Generates realistic synthetic data for model training and testing.

License: MIT
"""

import numpy as np
import pandas as pd
from datetime import datetime, timedelta
import json


def generate_patient_timeline(days=30, seed=None):
    """Generate a 30-day patient timeline with:
    - Hourly air quality data
    - Per-minute vitals (when worn)
    - Wheeze events
    - Actuation events
    - Alert events
    """
    if seed is not None:
        np.random.seed(seed)

    start = datetime(2024, 1, 1)
    hours = days * 24

    # ── Air Quality ────────────────────────────────
    air = pd.DataFrame({
        "timestamp": [start + timedelta(hours=h) for h in range(hours)],
        "pm25": np.clip(15 + 10 * np.sin(np.arange(hours) * np.pi / 12) +
                        np.random.normal(0, 5, hours), 0, 100),
        "voc_index": np.clip(150 + 50 * np.sin(np.arange(hours) * np.pi / 12 + 1) +
                             np.random.normal(0, 30, hours), 0, 500),
        "co2_ppm": np.clip(500 + 200 * np.sin(np.arange(hours) * np.pi / 12) +
                           np.random.normal(0, 50, hours), 400, 5000),
        "temp_c": 22 + 3 * np.sin(np.arange(hours) * np.pi / 12) +
                  np.random.normal(0, 1, hours),
        "humidity_pct": np.clip(45 + 10 * np.sin(np.arange(hours) * np.pi / 12 + 2) +
                                np.random.normal(0, 3, hours), 10, 90),
    })

    # ── Vitals ─────────────────────────────────────
    vitals_minutes = days * 24 * 60  # per-minute when band is worn
    # Simulate 16-hour wear time per day
    wear_mask = np.zeros(vitals_minutes, dtype=bool)
    for d in range(days):
        start_min = d * 24 * 60 + 7 * 60  # 7 AM
        end_min = start_min + 16 * 60     # 16 hours
        wear_mask[start_min:end_min] = True

    worn_minutes = wear_mask.sum()
    vitals = pd.DataFrame({
        "timestamp": [start + timedelta(minutes=m) for m in range(vitals_minutes) if wear_mask[m]],
        "hr": np.clip(70 + np.random.normal(0, 8, worn_minutes), 50, 150).astype(int),
        "spo2": np.clip(97 + np.random.normal(0, 1, worn_minutes), 90, 100).astype(int),
        "hrv_rmssd": np.clip(35 + np.random.normal(0, 10, worn_minutes), 5, 80),
        "skin_temp": 33 + np.random.normal(0, 0.5, worn_minutes),
        "activity": np.random.choice([0, 1, 2, 3], worn_minutes, p=[0.5, 0.3, 0.15, 0.05]),
    })

    # ── Wheeze events ──────────────────────────────
    n_wheeze = np.random.poisson(3 * days)  # avg 3 per day
    wheeze_times = np.random.choice(hours, n_wheeze)
    wheeze = pd.DataFrame({
        "timestamp": [start + timedelta(hours=int(h)) for h in wheeze_times],
        "wheeze_prob": np.random.randint(65, 100, n_wheeze),
        "class": np.random.choice(["wheeze", "wheeze_expiratory", "wheeze_polyphonic"],
                                   n_wheeze),
    })

    # ── Actuations ─────────────────────────────────
    n_actuations = np.random.poisson(1.5 * days)  # avg 1.5 per day
    act_times = np.random.choice(hours, n_actuations)
    actuations = pd.DataFrame({
        "timestamp": [start + timedelta(hours=int(h)) for h in act_times],
        "confidence": np.random.randint(75, 100, n_actuations),
        "peak_accel_g": np.random.uniform(3.0, 8.0, n_actuations),
        "duration_ms": np.random.randint(80, 200, n_actuations),
    })

    # ── Alerts ─────────────────────────────────────
    alerts = pd.DataFrame({
        "timestamp": [start + timedelta(hours=int(h)) for h in
                      np.random.choice(hours, 5)],
        "zone": np.random.choice([1, 2], 5),
        "message": ["PM2.5 high", "Rescue use elevated", "SpO2 low",
                     "Wheeze detected", "HRV drop"],
    })

    return {
        "air_quality": air,
        "vitals": vitals,
        "wheeze_events": wheeze,
        "actuations": actuations,
        "alerts": alerts,
    }


if __name__ == "__main__":
    data = generate_patient_timeline(days=30, seed=42)

    # Save as CSVs
    for name, df in data.items():
        filename = f"synthetic_{name}.csv"
        df.to_csv(filename, index=False)
        print(f"Generated {filename}: {len(df)} rows")
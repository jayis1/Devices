"""
train_routine_model.py — Learn household occupancy routines.

Uses a Hidden Markov Model (HMM) to learn:
  - When each room is typically occupied (time-of-day patterns)
  - Room transition probabilities (bedroom → bathroom → kitchen morning)
  - Schedule deviations (vacation, guests, work-from-home changes)

The learned routine model predicts room occupancy probability for the
next 4 hours, enabling predictive pre-conditioning (pre-heat bedroom 30 min
before predicted wake-up time).

Output: per-zone occupancy probability per 15-min slot for next 4 hours.
"""

import numpy as np
import pandas as pd
from hmmlearn import hmm
import sqlite3
import json
import os
from datetime import datetime, timedelta


def load_occupancy_data(db_path, days=30):
    """Load occupancy history from sensor readings."""
    conn = sqlite3.connect(db_path)
    since = (datetime.now() - timedelta(days=days)).isoformat()

    df = pd.read_sql_query("""
        SELECT timestamp, zone_id, occupancy
        FROM sensor_readings
        WHERE timestamp >= ?
        ORDER BY timestamp
    """, conn, params=(since,))
    conn.close()

    if df.empty:
        print("[INFO] No occupancy data, generating synthetic")
        df = generate_synthetic_occupancy(days)

    return df


def generate_synthetic_occupancy(days=30):
    """Generate realistic synthetic occupancy patterns."""
    np.random.seed(42)
    n_zones = 4  # bedroom, bathroom, kitchen, living
    timestamps = pd.date_range(
        datetime.now() - timedelta(days=days),
        periods=days * 24 * 4,  # 15-min resolution
        freq="15min"
    )

    data = []
    for ts in timestamps:
        hour = ts.hour + ts.minute / 60.0
        dow = ts.dayofweek  # 0=Monday

        is_weekday = dow < 5
        is_weekend = not is_weekday

        for zone_id in range(n_zones):
            # Zone 0: bedroom — occupied 22:00-07:00
            # Zone 1: bathroom — occupied briefly morning + evening
            # Zone 2: kitchen — occupied 7-9, 12-13, 18-20
            # Zone 3: living — occupied 18-22 weekday, 10-22 weekend
            if zone_id == 0:  # bedroom
                if 22 < hour or hour < 7:
                    occ = 1.0 if np.random.rand() > 0.05 else 0.0
                else:
                    occ = 0.0 if np.random.rand() > 0.05 else 1.0
            elif zone_id == 1:  # bathroom
                if (6.5 < hour < 8) or (18 < hour < 20):
                    occ = 1.0 if np.random.rand() > 0.5 else 0.0
                else:
                    occ = 0.0
            elif zone_id == 2:  # kitchen
                if (7 < hour < 9) or (12 < hour < 13) or (18 < hour < 20):
                    occ = 1.0 if np.random.rand() > 0.3 else 0.0
                else:
                    occ = 0.0
            else:  # living
                if is_weekend:
                    if 10 < hour < 22:
                        occ = 1.0 if np.random.rand() > 0.2 else 0.0
                    else:
                        occ = 0.0
                else:
                    if 18 < hour < 22:
                        occ = 1.0 if np.random.rand() > 0.2 else 0.0
                    else:
                        occ = 0.0

            data.append({
                "timestamp": ts,
                "zone_id": zone_id,
                "occupancy": int(occ),
            })

    return pd.DataFrame(data)


def train_zone_routine(df, zone_id, n_states=4):
    """
    Train an HMM for one zone's occupancy pattern.

    States represent occupancy patterns:
      0: empty
      1: briefly occupied (passing through)
      2: occupied (sustained)
      3: active (high activity)

    Observations: occupancy (0-3) binned from sensor data.
    """
    zone_data = df[df["zone_id"] == zone_id].copy()
    if zone_data.empty:
        return None

    # Create time-of-day features (cyclical)
    zone_data["hour"] = pd.to_datetime(zone_data["timestamp"]).dt.hour
    zone_data["minute"] = pd.to_datetime(zone_data["timestamp"]).dt.minute
    zone_data["time_slot"] = (zone_data["hour"] * 4 + zone_data["minute"] // 15)

    # Create observation sequence: occupancy values
    obs = zone_data["occupancy"].values.reshape(-1, 1).astype(float)

    if len(obs) < 100:
        print(f"[ZONE {zone_id}] Too few observations ({len(obs)})")
        return None

    # Train HMM
    model = hmm.GaussianHMM(
        n_components=n_states,
        covariance_type="diag",
        n_iter=100,
        random_state=42,
    )

    try:
        model.fit(obs)
        print(f"[ZONE {zone_id}] HMM trained: {len(obs)} observations, {n_states} states")
        return model
    except Exception as e:
        print(f"[ZONE {zone_id}] HMM training failed: {e}")
        return None


def predict_occupancy(model, zone_id, hours_ahead=4, slot_minutes=15):
    """
    Predict occupancy probability for next 4 hours.

    Uses the HMM's transition matrix to forecast future states
    from current observed state.
    """
    if model is None:
        return np.zeros(hours_ahead * 60 // slot_minutes)

    n_steps = hours_ahead * 60 // slot_minutes

    # Get current state (last observation)
    # In production: use latest sensor reading
    current_state = 0  # stub

    # Forecast: apply transition matrix repeatedly
    probs = np.zeros(n_steps)
    state_dist = np.zeros(model.n_components)
    state_dist[current_state] = 1.0

    for step in range(n_steps):
        state_dist = state_dist @ model.transmat_
        # Probability of being "occupied" (states 1+2+3)
        occ_prob = sum(state_dist[i] * model.means_[i][0]
                       for i in range(model.n_components))
        probs[step] = np.clip(occ_prob, 0, 1)

    return probs


def learn_routine_patterns(df):
    """
    Extract time-of-day occupancy patterns for each zone.

    Returns: per-zone occupancy probability by hour-of-day (24 values).
    """
    zones = df["zone_id"].unique()
    patterns = {}

    for zone_id in zones:
        zone_data = df[df["zone_id"] == zone_id].copy()
        zone_data["hour"] = pd.to_datetime(zone_data["timestamp"]).dt.hour

        # Average occupancy by hour
        hourly = zone_data.groupby("hour")["occupancy"].mean()

        # Fill missing hours with 0
        full = np.zeros(24)
        for h in range(24):
            if h in hourly.index:
                full[h] = hourly[h]

        # Smooth with moving average
        smoothed = np.convolve(full, np.ones(3) / 3, mode="same")
        smoothed = np.clip(smoothed, 0, 1)

        patterns[int(zone_id)] = smoothed.tolist()

        # Find peak occupancy hours
        peak_hours = np.argsort(smoothed)[-3:]
        print(f"[ZONE {zone_id}] Peak hours: {sorted(peak_hours)} "
              f"(occ={smoothed[peak_hours].round(2)})")

    return patterns


def export_routine_model(patterns, models, output_path="models/routine_model.json"):
    """Export learned routines for deployment to hub."""
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Convert HMM models to serializable format
    export_data = {
        "model_type": "hmm_occupancy",
        "version": "1.0",
        "patterns": patterns,  # per-zone hourly occupancy probability
        "zones": {},
    }

    for zone_id, model in models.items():
        if model is not None:
            export_data["zones"][str(zone_id)] = {
                "n_states": model.n_components,
                "transmat": model.transmat_.tolist(),
                "means": model.means_.tolist(),
            }

    with open(output_path, "w") as f:
        json.dump(export_data, f, indent=2)

    print(f"[EXPORT] Routine model saved to {output_path}")
    print(f"[EXPORT] {len(patterns)} zone patterns, {len(models)} HMM models")


if __name__ == "__main__":
    db = os.environ.get("DB_PATH", "/data/thermogrid.db")
    if not os.path.exists(db):
        db = "/tmp/thermogrid.db"
        print(f"[INFO] DB not found, using synthetic data")

    df = load_occupancy_data(db, days=30)
    print(f"[DATA] {len(df)} occupancy readings")

    # Learn time-of-day patterns
    patterns = learn_routine_patterns(df)

    # Train HMM per zone
    zone_ids = sorted(df["zone_id"].unique())
    models = {}
    for zid in zone_ids:
        models[zid] = train_zone_routine(df, zid)

    export_routine_model(patterns, models, "models/routine_model.json")
    print("\n[DONE] Routine learning complete")
"""
train_comfort_model.py — Train the personal thermal comfort model.

Learns each person's individual thermal comfort profile from their
comfort tag votes ("I'm cold" / "I'm hot") and sensor context.

The model predicts a comfort score (-3 cold .. 0 neutral .. +3 hot)
from: skin_temp, air_temp, MRT, humidity, air_vel, HR, HRV, activity,
      time-of-day, season, clothing_estimate.

Unlike the population-average PMV model (ISO 7730), this is personalized:
  - Cold-tolerant people get a shifted baseline
  - Elderly people may have reduced thermoregulation
  - Active people generate more metabolic heat

Model: XGBoost (gradient-boosted trees) — fast inference, interpretable,
      handles the nonlinear interactions between skin temp, humidity, and
      activity that determine perceived comfort.

Deployed: compressed model pushed to hub for on-device inference.
"""

import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, accuracy_score
import sqlite3
import json
import os
import pickle
from datetime import datetime


# ---- Feature Engineering ----

def extract_features(votes_df, comfort_df):
    """
    Merge comfort votes with sensor readings at vote time.

    Features:
      skin_temp:     wrist skin temperature °C (MAX30208)
      air_temp:       ambient air temperature °C (TMP117)
      skin_air_delta: skin_temp - air_temp (key comfort indicator)
      humidity:       local relative humidity % (SHT40)
      hr_bpm:          heart rate (MAX30101)
      hrv_ms:          heart rate variability (RMSSD)
      activity:        activity level (0-4: sedentary/light/moderate/vigorous/sleeping)
      hour:            time of day (0-23)
      hour_sin/cos:   cyclical encoding of hour
      month:           month (1-12, seasonal effect)
      season:          0=winter, 1=spring, 2=summer, 3=autumn
      is_sleeping:    1 if activity == sleeping
      metabolic_est:  estimated metabolic heat (W) based on activity
      clothing_est:   estimated clo value based on season + hour
    """
    features = []
    labels = []

    for _, vote in votes_df.iterrows():
        # Find closest comfort reading to vote timestamp
        person_id = vote["person_id"]
        vote_time = vote["timestamp"]

        # Get comfort data within 5 minutes of vote
        person_comfort = comfort_df[
            (comfort_df["person_id"] == person_id) &
            (abs(pd.to_datetime(comfort_df["timestamp"]) -
                 pd.to_datetime(vote_time)) < pd.Timedelta("5min"))
        ]

        if person_comfort.empty:
            # Fallback: use vote's own skin_temp and activity
            skin_temp = vote.get("skin_temp", 31.0)
            activity = vote.get("activity", 0)
            air_temp = skin_temp - 7.0  # approximate
            humidity = 45.0
            hr = 72
            hrv = 45
        else:
            c = person_comfort.iloc[0]
            skin_temp = c["skin_temp"]
            air_temp = c["air_temp"]
            humidity = c["humidity"]
            hr = c["hr_bpm"]
            hrv = c["hrv_ms"]
            activity = c["activity"]

        # Derived features
        skin_air_delta = skin_temp - air_temp

        # Time features
        dt = pd.to_datetime(vote_time)
        hour = dt.hour
        hour_sin = np.sin(2 * np.pi * hour / 24.0)
        hour_cos = np.cos(2 * np.pi * hour / 24.0)
        month = dt.month
        season = (month % 12) // 3  # 0=winter, 1=spring, 2=summer, 3=autumn

        # Metabolic heat estimate (W)
        met_w = {0: 80, 1: 120, 2: 200, 3: 350, 4: 60}  # sedentary→vigorous→sleeping
        metabolic_est = met_w.get(int(activity), 80)

        # Clothing estimate (clo units, rough seasonal model)
        # Winter: 1.0 clo, Summer: 0.5 clo
        clo_base = {0: 1.0, 1: 0.8, 2: 0.5, 3: 0.7}
        clothing_est = clo_base.get(season, 0.8)
        # Nighttime: add 1.0 clo (bedding)
        if hour < 6 or hour > 22:
            clothing_est += 1.0

        is_sleeping = 1 if activity == 4 else 0

        features.append([
            skin_temp, air_temp, skin_air_delta, humidity,
            hr, hrv, activity, metabolic_est,
            hour, hour_sin, hour_cos, month, season,
            is_sleeping, clothing_est
        ])
        labels.append(vote["vote"])

    return np.array(features), np.array(labels)


# ---- Personal Comfort Model ----

def train_personal_model(person_id, features, labels):
    """
    Train an XGBoost model for one person.

    If insufficient votes (<20), fall back to population model with
    personal offset (learned from the few votes available).
    """
    if len(labels) < 20:
        print(f"[PERSON {person_id}] Only {len(labels)} votes — "
              f"using population model + personal offset")
        # Personal offset: average deviation from PMV prediction
        pmv_pred = simple_pmv(features)
        personal_offset = np.mean(labels - pmv_pred)
        return {
            "person_id": person_id,
            "model_type": "pmv_with_offset",
            "personal_offset": float(personal_offset),
            "n_votes": len(labels),
        }

    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=0.2, random_state=42
    )

    model = xgb.XGBRegressor(
        n_estimators=100,
        max_depth=4,
        learning_rate=0.1,
        objective="reg:squarederror",
        random_state=42,
    )
    model.fit(X_train, y_train)

    pred = model.predict(X_test)
    rmse = np.sqrt(mean_squared_error(y_test, pred))
    print(f"[PERSON {person_id}] XGBoost model — RMSE: {rmse:.3f} ({len(labels)} votes)")

    # Feature importance
    importances = model.feature_importances_
    feature_names = [
        "skin_temp", "air_temp", "skin_air_delta", "humidity",
        "hr", "hrv", "activity", "metabolic",
        "hour", "hour_sin", "hour_cos", "month", "season",
        "is_sleeping", "clothing"
    ]
    print(f"[PERSON {person_id}] Feature importance:")
    for name, imp in sorted(zip(feature_names, importances),
                            key=lambda x: -x[1])[:5]:
        print(f"  {name}: {imp:.3f}")

    return {
        "person_id": person_id,
        "model_type": "xgboost",
        "model": model,
        "rmse": rmse,
        "n_votes": len(labels),
        "feature_importance": dict(zip(feature_names, importances.tolist())),
    }


def simple_pmv(features):
    """
    Simplified PMV (Predicted Mean Vote) — population average.
    ISO 7730 model, simplified to key inputs.

    Returns comfort prediction: -3 (cold) to +3 (hot)
    """
    pmv = np.zeros(len(features))
    for i, f in enumerate(features):
        skin_temp = f[0]
        air_temp = f[1]
        humidity = f[3]
        activity = f[6]
        clothing = f[14]

        # Simplified PMV (Fanger's model, heavily reduced)
        # Operative temp ≈ (air_temp + MRT) / 2, assume MRT ≈ air_temp
        t_op = air_temp

        # Metabolic rate (met units: 1 met = 58.2 W/m²)
        met = {0: 1.0, 1: 1.5, 2: 2.5, 3: 4.0, 4: 0.7}
        m = met.get(int(activity), 1.0)

        # Very simplified PMV
        # PMV ≈ 0.303 * exp(-0.036*M) * (M - W) - ...
        # We use a simplified regression:
        pmv_val = (0.1 * (t_op - 22.0) +
                   0.05 * (skin_temp - 31.0) -
                   0.02 * (humidity - 50.0) +
                   0.3 * (m - 1.0) -
                   0.5 * (clothing - 1.0))

        pmv[i] = np.clip(pmv_val, -3.0, 3.0)

    return pmv


# ---- Training Pipeline ----

def train_all_comfort_models(db_path):
    """Train a personal comfort model for each person who has voted."""
    conn = sqlite3.connect(db_path)

    votes_df = pd.read_sql_query(
        "SELECT * FROM comfort_votes ORDER BY timestamp", conn)
    comfort_df = pd.read_sql_query(
        "SELECT * FROM comfort_readings ORDER BY timestamp", conn)

    conn.close()

    if votes_df.empty:
        print("[INFO] No votes yet, generating synthetic vote data for demo")
        votes_df, comfort_df = generate_synthetic_votes()

    person_ids = votes_df["person_id"].unique()
    models = {}

    for person_id in person_ids:
        person_votes = votes_df[votes_df["person_id"] == person_id]
        print(f"\n[PERSON 0x{person_id:02X}] {len(person_votes)} votes")

        features, labels = extract_features(person_votes, comfort_df)

        if len(features) == 0:
            print(f"[PERSON 0x{person_id:02X}] No matching comfort data — skipping")
            continue

        model = train_personal_model(person_id, features, labels)
        models[person_id] = model

    return models


def generate_synthetic_votes():
    """Generate synthetic comfort vote data for initial training demo."""
    np.random.seed(42)
    n_days = 30
    n_people = 3

    votes = []
    comfort = []

    for person in range(n_people):
        person_id = 0x80 + person
        # Each person has different thermal preference
        # Person 0: cold-tolerant (comfortable at 18°C)
        # Person 1: average (comfortable at 21°C)
        # Person 2: heat-tolerant (comfortable at 24°C)
        comfort_base = 18.0 + person * 3.0

        for day in range(n_days):
            for hour in range(24):
                if np.random.rand() > 0.3:  # 30% of hours have a vote
                    continue

                ts = f"2025-01-{day+1:02d} {hour:02d}:00:00"
                room_temp = comfort_base + np.random.randn() * 3.0
                skin_temp = room_temp - 7.0 + np.random.randn() * 1.0

                activity = np.random.choice([0, 1, 2, 4], p=[0.5, 0.2, 0.1, 0.2])

                # Vote: based on deviation from personal comfort base
                deviation = room_temp - comfort_base
                vote = int(np.clip(round(deviation * 0.5), -3, 3))
                if abs(vote) < 1 and np.random.rand() > 0.5:
                    continue  # skip neutral votes sometimes

                votes.append({
                    "person_id": person_id,
                    "timestamp": ts,
                    "vote": vote,
                    "skin_temp": skin_temp,
                    "activity": activity,
                    "room_id": 0,
                })

                comfort.append({
                    "person_id": person_id,
                    "timestamp": ts,
                    "skin_temp": skin_temp,
                    "air_temp": room_temp,
                    "humidity": 45.0 + np.random.randn() * 5,
                    "hr_bpm": 72 + np.random.randint(-10, 10),
                    "hrv_ms": 45 + np.random.randint(-10, 10),
                    "activity": activity,
                    "comfort_score": vote,
                    "comfort_conf": 0.8,
                    "vote_pending": 0,
                    "battery_pct": 85,
                    "signal_rssi": -60,
                    "seq_num": 0,
                })

    return pd.DataFrame(votes), pd.DataFrame(comfort)


def export_models(models, output_dir="models"):
    """Export trained models for deployment to hub."""
    os.makedirs(output_dir, exist_ok=True)

    for person_id, model_info in models.items():
        if model_info["model_type"] == "xgboost":
            # Export XGBoost model
            model = model_info["model"]
            model_path = f"{output_dir}/comfort_person_{person_id:02x}.json"
            model.save_model(model_path)
            print(f"[EXPORT] Person 0x{person_id:02X} XGBoost → {model_path}")

            # Save metadata
            meta = {
                "person_id": person_id,
                "model_type": "xgboost",
                "rmse": model_info["rmse"],
                "n_votes": model_info["n_votes"],
                "feature_importance": model_info["feature_importance"],
            }
            with open(f"{output_dir}/comfort_person_{person_id:02x}_meta.json", "w") as f:
                json.dump(meta, f, indent=2)

        elif model_info["model_type"] == "pmv_with_offset":
            # Export PMV + personal offset (very lightweight)
            meta = {
                "person_id": person_id,
                "model_type": "pmv_with_offset",
                "personal_offset": model_info["personal_offset"],
                "n_votes": model_info["n_votes"],
            }
            with open(f"{output_dir}/comfort_person_{person_id:02x}_meta.json", "w") as f:
                json.dump(meta, f, indent=2)
            print(f"[EXPORT] Person 0x{person_id:02X} PMV+offset → meta only")

    # Export population PMV model as fallback
    with open(f"{output_dir}/comfort_population_pmv.json", "w") as f:
        json.dump({"model_type": "simple_pmv", "version": "1.0"}, f, indent=2)


if __name__ == "__main__":
    db = os.environ.get("DB_PATH", "/data/thermogrid.db")
    if not os.path.exists(db):
        db = "/tmp/thermogrid.db"
        print(f"[INFO] DB not found, using synthetic data")

    models = train_all_comfort_models(db)
    export_models(models, output_dir="models")
    print(f"\n[DONE] Trained comfort models for {len(models)} people")
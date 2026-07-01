"""
MigraineSync — Training Data Collection Script
================================================
Collects labeled training data from deployed MigraineSync systems.
Fetches telemetry from the cloud backend + manual event labels,
and prepares it for model retraining.

License: MIT
"""

import os
import json
import requests
import pandas as pd
from datetime import datetime, timedelta

API_BASE = os.getenv("MIGRAINESYNC_API", "https://api.migrainesync.io/api/v1")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline", "data")
os.makedirs(OUTPUT_DIR, exist_ok=True)


def collect_trends(metric: str, hours: int = 24 * 90) -> list:
    """Fetch 90 days of trend data for a given metric."""
    try:
        resp = requests.get(f"{API_BASE}/trends",
                           params={"metric": metric, "hours": min(hours, 720)},
                           timeout=30)
        resp.raise_for_status()
        return resp.json().get("data", [])
    except Exception as e:
        print(f"Failed to fetch {metric}: {e}")
        return []


def collect_events(limit: int = 500) -> list:
    """Fetch recent events (migraine logs, alerts)."""
    try:
        resp = requests.get(f"{API_BASE}/events", params={"limit": limit}, timeout=30)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        print(f"Failed to fetch events: {e}")
        return []


def build_training_dataset():
    """Build a training dataset from collected data."""
    print("Collecting training data from MigraineSync backend...")

    metrics = ["hrv", "pressure", "light", "hydration", "skin_temp", "activity"]
    all_data = {}

    for metric in metrics:
        print(f"  Fetching {metric}...")
        data = collect_trends(metric)
        all_data[metric] = data
        print(f"    {len(data)} data points")

    print("  Fetching events (migraine labels)...")
    events = collect_events()
    migraine_events = [e for e in events if e.get("event_type") in ("migraine_onset", "manual")]
    print(f"    {len(migraine_events)} migraine events")

    # Merge into a single dataframe
    dfs = []
    for metric, data in all_data.items():
        if data:
            df = pd.DataFrame(data)
            df["metric"] = metric
            dfs.append(df)

    if dfs:
        combined = pd.concat(dfs, ignore_index=True)
        output_path = os.path.join(OUTPUT_DIR, f"collected_data_{datetime.now().strftime('%Y%m%d')}.csv")
        combined.to_csv(output_path, index=False)
        print(f"\nDataset saved: {output_path} ({len(combined)} rows)")
    else:
        print("\nNo data collected. Ensure the backend is running and has data.")

    # Save events as labels
    if migraine_events:
        events_df = pd.DataFrame(migraine_events)
        events_path = os.path.join(OUTPUT_DIR, f"migraine_labels_{datetime.now().strftime('%Y%m%d')}.csv")
        events_df.to_csv(events_path, index=False)
        print(f"Labels saved: {events_path} ({len(events_df)} events)")

    print("\nDone! Use this data for model retraining via scripts/train_all.sh")


if __name__ == "__main__":
    build_training_dataset()
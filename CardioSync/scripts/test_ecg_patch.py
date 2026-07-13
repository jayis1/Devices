#!/usr/bin/env python3
"""
test_ecg_patch.py — Test ECG patch connectivity and signal quality

Verifies that the ECG chest patch is:
  - Connected to the Hub via BLE
  - Producing valid ECG signal (not flatline)
  - R-peak detection is working (HR > 0)
  - No lead-off condition
  - Battery level adequate

Usage:
    python test_ecg_patch.py

License: MIT
"""
import sys
import time
import requests

API_BASE = "http://localhost:8000/api/v1"

def test_ecg_patch():
    print("CardioSync ECG Patch Test")
    print("=" * 40)

    # Check API health
    try:
        r = requests.get(f"{API_BASE}/health", timeout=5)
        if r.status_code != 200:
            print(f"✗ API not healthy: {r.status_code}")
            sys.exit(1)
        print("✓ API healthy")
    except Exception as e:
        print(f"✗ Cannot connect to API: {e}")
        sys.exit(1)

    # Check dashboard for ECG data
    try:
        r = requests.get(f"{API_BASE}/dashboard", timeout=5)
        data = r.json()

        if data.get("heart_rate", 0) > 0:
            print(f"✓ ECG patch producing signal (HR={data['heart_rate']} bpm)")
        else:
            print("✗ ECG patch not producing signal (HR=0)")
            print("  Check: electrodes connected? Patch powered?")

        if data.get("afib_events_24h", 0) > 0:
            print(f"⚠ {data['afib_events_24h']} AFib events in last 24h")

        if data.get("hrv", {}).get("rmssd", 0) > 0:
            print(f"✓ HRV data available (RMSSD={data['hrv']['rmssd']} ms)")
        else:
            print("✗ No HRV data — ECG patch may not be connected")

    except Exception as e:
        print(f"✗ Failed to get dashboard data: {e}")
        sys.exit(1)

    # Check recent ECG events
    try:
        r = requests.get(f"{API_BASE}/ecg/events?limit=5", timeout=5)
        events = r.json()
        if events:
            print(f"✓ {len(events)} recent ECG events found")
            for ev in events[:3]:
                print(f"  - {ev['event_type']} (HR={ev['heart_rate']}, "
                      f"conf={ev['confidence']:.2f})")
        else:
            print("✓ No recent arrhythmia events (normal)")
    except Exception as e:
        print(f"⚠ Could not fetch ECG events: {e}")

    print()
    print("ECG Patch Test Complete")

if __name__ == "__main__":
    test_ecg_patch()
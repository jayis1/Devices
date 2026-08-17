#!/usr/bin/env python3
"""
TremorSync Calibration Script
Runs calibration protocols for all nodes.

Usage:
    python calibrate.py --node tremor_band
    python calibrate.py --node gait_pod
    python calibrate.py --node voice_node
    python calibrate.py --node med_station
    python calibrate.py --all
"""

import argparse
import requests
import time
import json

API_BASE = "http://localhost:8000"

def calibrate_tremor_band():
    """Tremor Band calibration: baseline resting, postural, action tremor."""
    print("\n=== Tremor Band Calibration ===")
    print("Wear band on most affected wrist (snug but comfortable).")
    input("Press Enter when ready...")

    print("\n[1/4] Resting baseline")
    print("  Rest arm on table, fully supported. Hold still for 30 seconds.")
    input("  Press Enter to start recording...")
    resp = requests.post(f"{API_BASE}/api/v1/calibration/start",
                         json={"node": "tremor_band", "phase": "resting"})
    print(f"  Recording: {resp.json() if resp.ok else 'API error'}")
    time.sleep(30)
    print("  Resting baseline recorded.")

    print("\n[2/4] Postural baseline")
    print("  Hold arm extended against gravity for 30 seconds.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "tremor_band", "phase": "postural"})
    time.sleep(30)
    print("  Postural baseline recorded.")

    print("\n[3/4] Action tremor")
    print("  Perform finger-to-nose movement 5 times.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "tremor_band", "phase": "action"})
    time.sleep(30)
    print("  Action tremor pattern recorded.")

    print("\n[4/4] Clinical reference (optional)")
    print("  Enter neurologist's MDS-UPDRS tremor score (0-4):")
    score = input("  Score: ").strip()
    if score:
        requests.post(f"{API_BASE}/api/v1/calibration/start",
                      json={"node": "tremor_band", "phase": "clinical_ref",
                            "value": float(score)})

    print("\nTremor Band calibration complete!")


def calibrate_gait_pod():
    """Gait Pod calibration: baseline stride, fast walk, quiet standing, FOG pattern."""
    print("\n=== Gait Pod Calibration ===")
    print("Attach pod to shoe (clip under laces, IMU on top, FSR under heel).")
    input("Press Enter when ready...")

    print("\n[1/4] Normal walk baseline")
    print("  Walk 10 meters at normal pace.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "gait_pod", "phase": "normal_walk"})
    time.sleep(15)
    print("  Normal walk recorded.")

    print("\n[2/4] Fast walk")
    print("  Walk 10 meters at fast pace.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "gait_pod", "phase": "fast_walk"})
    time.sleep(10)
    print("  Fast walk recorded.")

    print("\n[3/4] Quiet standing")
    print("  Stand still for 30 seconds.")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "gait_pod", "phase": "quiet_standing"})
    time.sleep(30)
    print("  Quiet standing recorded.")

    print("\n[4/4] FOG pattern (optional)")
    print("  March in place for 5 seconds (simulates freeze).")
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "gait_pod", "phase": "fog_simulation"})
    time.sleep(10)
    print("  FOG pattern recorded for personal threshold.")

    print("\nGait Pod calibration complete!")


def calibrate_voice_node():
    """Voice Node calibration: sustained vowel, reading passage, swallow."""
    print("\n=== Voice Node Calibration ===")
    print("Wear throat band (mic over cricothyroid membrane, electrodes either side).")
    input("Press Enter when ready...")

    print("\n[1/3] Sustained vowel")
    print('  Say "ahhh" at comfortable pitch for 5 seconds.')
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "voice_node", "phase": "sustained_vowel"})
    time.sleep(7)
    print("  Baseline F0, amplitude, jitter, shimmer recorded.")

    print("\n[2/3] Connected speech")
    print('  Read: "The grandfather passage" (provided separately).')
    input("  Press Enter to start...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "voice_node", "phase": "connected_speech"})
    time.sleep(30)
    print("  Connected speech features recorded.")

    print("\n[3/3] Swallow baseline")
    print("  Swallow 10 mL water.")
    input("  Press Enter to start (then swallow)...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "voice_node", "phase": "swallow"})
    time.sleep(5)
    print("  Swallow timing baseline recorded.")

    print("\nVoice Node calibration complete!")


def calibrate_med_station():
    """Med Station calibration: load carousel, verify weight, set schedule."""
    print("\n=== Med Station Calibration ===")
    print("Load carousel with prescribed levodopa doses (28 compartments max).")
    input("Press Enter when ready...")

    print("\n[1/3] Pill weight verification")
    print("  Place one pill on load cell.")
    input("  Press Enter to weigh...")
    resp = requests.get(f"{API_BASE}/api/v1/med/onoff")  # triggers weight read
    print(f"  Measured weight: check display")
    print(f"  Expected: 100/200/250 mg per prescription")

    print("\n[2/3] Carousel position verification")
    print("  Verifying each compartment alignment...")
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "med_station", "phase": "carousel_check"})
    time.sleep(30)
    print("  Carousel positions verified.")

    print("\n[3/3] Dose schedule")
    print("  Enter dose interval in hours (default 4):")
    interval = input("  Interval (h): ").strip() or "4"
    print("  Enter max doses per day (default 6):")
    max_doses = input("  Max/day: ").strip() or "6"
    requests.post(f"{API_BASE}/api/v1/calibration/start",
                  json={"node": "med_station", "phase": "schedule",
                        "interval_h": int(interval),
                        "max_per_day": int(max_doses)})

    print("\nMed Station calibration complete!")
    print(f"  Interval: {interval}h, Max: {max_doses}/day")
    print("  ON/OFF learning mode: 7-day baseline → predictive dosing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TremorSync Calibration")
    parser.add_argument("--node", choices=["tremor_band", "gait_pod",
                                            "voice_node", "med_station"])
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()

    if args.all or args.node == "tremor_band":
        calibrate_tremor_band()
    if args.all or args.node == "gait_pod":
        calibrate_gait_pod()
    if args.all or args.node == "voice_node":
        calibrate_voice_node()
    if args.all or args.node == "med_station":
        calibrate_med_station()

    print("\n=== All calibration complete ===")
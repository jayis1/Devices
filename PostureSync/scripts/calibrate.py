#!/usr/bin/env python3
"""
PostureSync Calibration Script
Guides the user through calibration of each node.

Usage: python calibrate.py --node [spine_band|posture_garment|chair_pad|all]
"""

import argparse
import requests
import time
import sys

API_BASE = "http://localhost:8000/api/v1"


def calibrate_spine_band():
    """Calibrate the Spine Band (worn between shoulder blades)"""
    print("\n=== Spine Band Calibration ===")
    print("1. Wear the band between your shoulder blades (clip to shirt)")
    print("2. Stand in neutral posture against a wall")
    print("   (heels, buttocks, shoulders, and head touching the wall)")
    input("Press Enter when ready...")

    print("Recording neutral posture (2 seconds)...")
    for i in range(2, 0, -1):
        print(f"  {i}...")
        time.sleep(1)

    print("3. Bend forward 30 degrees")
    input("Press Enter when bent forward...")
    print("Recording forward limit (2 seconds)...")
    time.sleep(2)

    print("4. Lean backward 10 degrees")
    input("Press Enter when leaning back...")
    print("Recording backward limit (2 seconds)...")
    time.sleep(2)

    print("5. Lean left 20 degrees")
    input("Press Enter when leaning left...")
    print("Recording left limit (2 seconds)...")
    time.sleep(2)

    print("6. Lean right 20 degrees")
    input("Press Enter when leaning right...")
    print("Recording right limit (2 seconds)...")
    time.sleep(2)

    # Send calibration command to Hub
    try:
        resp = requests.post(f"{API_BASE}/calibration/start",
                           json={"node_id": 2, "calibration_type": "full"})
        print(f"Calibration sent: {resp.status_code}")
    except Exception as e:
        print(f"Warning: Could not reach API: {e}")

    print("✅ Spine Band calibration complete!")


def calibrate_posture_garment():
    """Calibrate the Posture Garment (smart shirt)"""
    print("\n=== Posture Garment Calibration ===")
    print("1. Put on the Posture Garment (compression fit)")
    print("   Ensure all 8 electrodes make skin contact")
    input("Press Enter when worn...")

    print("2. Stand in neutral posture")
    print("Recording EMG baseline (2 seconds)...")
    time.sleep(2)

    muscles = [
        ("Upper Trapezius", "Shrug your shoulders up and hold"),
        ("Erector Spinae", "Arch your back slightly and hold"),
        ("Sternocleidomastoid", "Turn your head to each side"),
        ("Rectus Abdominis", "Flex your abs and hold"),
    ]

    for name, instruction in muscles:
        print(f"3. Contract {name}")
        print(f"   Instruction: {instruction}")
        input("   Press Enter when contracting...")
        print(f"   Recording MVC (3 seconds)...")
        time.sleep(3)

    try:
        resp = requests.post(f"{API_BASE}/calibration/start",
                           json={"node_id": 3, "calibration_type": "full"})
        print(f"Calibration sent: {resp.status_code}")
    except Exception as e:
        print(f"Warning: Could not reach API: {e}")

    print("✅ Posture Garment calibration complete!")


def calibrate_chair_pad():
    """Calibrate the Smart Chair Pad"""
    print("\n=== Smart Chair Pad Calibration ===")
    print("1. Place the pad on your chair")
    print("2. Sit centered on the pad with feet flat on floor")
    input("Press Enter when seated...")

    print("Recording neutral weight distribution (5 seconds)...")
    time.sleep(5)

    print("3. Lean to the left")
    input("Press Enter when leaning left...")
    print("Recording left lean (3 seconds)...")
    time.sleep(3)

    print("4. Lean to the right")
    input("Press Enter when leaning right...")
    print("Recording right lean (3 seconds)...")
    time.sleep(3)

    print("5. Slouch (sacral sitting)")
    input("Press Enter when slouching...")
    print("Recording slouch pattern (3 seconds)...")
    time.sleep(3)

    print("6. Stand up from the chair")
    input("Press Enter when standing...")
    print("Recording zero-load baseline (3 seconds)...")
    time.sleep(3)

    try:
        resp = requests.post(f"{API_BASE}/calibration/start",
                           json={"node_id": 4, "calibration_type": "full"})
        print(f"Calibration sent: {resp.status_code}")
    except Exception as e:
        print(f"Warning: Could not reach API: {e}")

    print("✅ Chair Pad calibration complete!")


def main():
    parser = argparse.ArgumentParser(description="PostureSync Calibration")
    parser.add_argument("--node", choices=["spine_band", "posture_garment",
                                           "chair_pad", "all"],
                       default="all", help="Node to calibrate")
    parser.add_argument("--api", default=API_BASE, help="API base URL")
    args = parser.parse_args()

    global API_BASE
    API_BASE = args.api

    print("PostureSync Calibration Tool")
    print("=============================")

    if args.node in ("spine_band", "all"):
        calibrate_spine_band()

    if args.node in ("posture_garment", "all"):
        calibrate_posture_garment()

    if args.node in ("chair_pad", "all"):
        calibrate_chair_pad()

    print("\n✅ All calibrations complete!")
    print("Your PostureSync system is ready to use.")


if __name__ == "__main__":
    main()
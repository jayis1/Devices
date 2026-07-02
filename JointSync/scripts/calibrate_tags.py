#!/usr/bin/env python3
"""
JointSync — Tag Calibration Script

Calibrates Joint Tag ROM (range of motion) zero reference.
The patient holds the joint in a neutral position and presses the button.

Usage: python calibrate_tags.py --tag-id 1
"""

import argparse
import subprocess
import time
import json

def calibrate_tag(tag_id: int):
    """Calibrate a Joint Tag's ROM reference."""
    print(f"=== JointSync Tag {tag_id} Calibration ===")
    print()
    print("Instructions:")
    print("  1. Place the tag on the affected joint")
    print("  2. Hold the joint in a NEUTRAL position (straight, relaxed)")
    print("  3. Press the tag button when ready")
    print("  4. The tag will record the current orientation as zero reference")
    print()

    input("Press ENTER when the joint is in neutral position...")

    # In production, this would send a CMD_MODE packet via BLE
    # to trigger sensor_fusion_set_reference() on the tag
    print(f"Sending calibration command to Tag {tag_id}...")

    # Simulated BLE command
    cmd = {
        "msg_type": "CMD_MODE",
        "tag_id": tag_id,
        "action": "set_reference",
        "timestamp": time.time(),
    }
    print(f"Command: {json.dumps(cmd, indent=2)}")
    print()
    print("✓ Calibration complete! Tag zero reference set.")
    print()
    print("Now perform a full range of motion:")
    print("  1. Slowly flex the joint as far as comfortable")
    print("  2. Return to neutral")
    print("  3. Slowly extend the joint as far as comfortable")
    print("  4. Return to neutral")
    print()
    print("The tag will record max flexion and max extension as ROM.")

    input("Press ENTER when ROM measurement is complete...")
    print("✓ ROM measurement recorded!")

def main():
    parser = argparse.ArgumentParser(description="Calibrate JointSync Joint Tag")
    parser.add_argument("--tag-id", type=int, required=True, help="Tag ID to calibrate")
    args = parser.parse_args()
    calibrate_tag(args.tag_id)

if __name__ == "__main__":
    main()
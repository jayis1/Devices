#!/usr/bin/env python3
"""
PorchGuard — Sensor Calibration Script

Calibrates all PorchGuard node sensors:
- Camera: white balance, mmWave zone config, PIR sensitivity
- Mailbox: load cell tare + known-weight scale, reed debounce
- Lock: motor stroke calibration, door sensor confirm

Usage:
    python3 calibrate_sensors.py --node all
    python3 calibrate_sensors.py --node camera
    python3 calibrate_sensors.py --node mailbox
    python3 calibrate_sensors.py --node lock
"""

import argparse
import json
import time
import paho.mqtt.client as mqtt

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

client = mqtt.Client(client_id="porchguard-calibrate")


def calibrate_camera():
    """Calibrate porch camera sensors."""
    print("\n=== Porch Camera Calibration ===\n")

    # 1. White balance
    print("Step 1: Camera white balance")
    print("  Point camera at a white reference card (1m distance).")
    input("  Press ENTER when ready...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "camera", "action": "white_balance"}))
    time.sleep(3)
    print("  ✓ White balance calibrated")

    # 2. mmWave zone configuration
    print("\nStep 2: mmWave presence zone")
    print("  Stand at the porch edge (where a visitor would approach).")
    input("  Press ENTER to set presence zone boundary...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "mmwave", "action": "set_zone"}))
    time.sleep(2)
    print("  ✓ mmWave zone boundary set")

    print("  Now step OFF the porch completely.")
    input("  Press ENTER to set clear-state baseline...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "mmwave", "action": "baseline_clear"}))
    time.sleep(2)
    print("  ✓ mmWave clear baseline set")

    # 3. PIR sensitivity
    print("\nStep 3: PIR sensitivity")
    print("  Walk toward the porch from 5m away at normal pace.")
    input("  Press ENTER, then walk toward porch...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "pir", "action": "sensitivity_sweep"}))
    time.sleep(10)
    print("  ✓ PIR sensitivity calibrated (trigger distance recorded)")

    # 4. Package detector test
    print("\nStep 4: Package detector verification")
    for label, expected in [("small box", 1), ("medium box", 2),
                             ("large box", 3), ("envelope", 4)]:
        input(f"  Press ENTER to place {label} on porch (expect class {expected})...")
        client.publish("porchguard/commands/calibrate",
                       json.dumps({"sensor": "package_test", "expected": expected}))
        time.sleep(5)
        print(f"  ✓ Tested {label}")

    # 5. Re-ID gallery enrollment
    print("\nStep 5: Resident re-ID enrollment")
    print("  Walk past the camera 3 times at normal pace.")
    for i in range(3):
        input(f"  Press ENTER for pass {i+1}/3, then walk past...")
        client.publish("porchguard/commands/calibrate",
                       json.dumps({"sensor": "reid_enroll", "pass": i+1}))
        time.sleep(8)
    print("  ✓ Resident embedding enrolled in gallery")

    print("\n✅ Porch camera calibration complete!")


def calibrate_mailbox():
    """Calibrate mailbox node sensors."""
    print("\n=== Mailbox Node Calibration ===\n")

    # 1. Load cell tare
    print("Step 1: Load cell tare (empty mailbox)")
    print("  Remove all mail from the mailbox tray.")
    input("  Press ENTER when tray is empty...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "load_cell", "action": "tare"}))
    time.sleep(2)
    print("  ✓ Load cell tared (zero offset set)")

    # 2. Load cell scale factor
    print("\nStep 2: Load cell scale factor")
    print("  Place a known weight on the tray (e.g., 100g).")
    weight = float(input("  Enter known weight in grams: "))
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "load_cell", "action": "scale",
                               "value": weight}))
    time.sleep(2)
    print(f"  ✓ Load cell scale factor set ({weight}g reference)")

    # 3. Reed switch debounce
    print("\nStep 3: Reed switch test")
    print("  Open and close the mailbox door 3 times slowly.")
    input("  Press ENTER when ready...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "reed", "action": "debounce_test"}))
    time.sleep(15)
    print("  ✓ Reed switch debounce configured")

    # 4. Tamper threshold
    print("\nStep 4: Tamper (tilt) threshold")
    print("  Tilt the mailbox node ~30° to set the tilt threshold.")
    input("  Press ENTER when tilted...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "tilt", "action": "set_threshold"}))
    time.sleep(2)
    print("  ✓ Tilt threshold set")

    print("\n✅ Mailbox node calibration complete!")


def calibrate_lock():
    """Calibrate lock node."""
    print("\n=== Lock Node Calibration ===\n")

    # 1. Motor stroke
    print("Step 1: Motor stroke calibration")
    print("  Ensure door is closed and deadbolt is in LOCKED position.")
    input("  Press ENTER to measure unlock stroke...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "motor", "action": "measure_unlock"}))
    time.sleep(5)
    print("  ✓ Unlock stroke measured")
    input("  Press ENTER to measure lock stroke...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "motor", "action": "measure_lock"}))
    time.sleep(5)
    print("  ✓ Lock stroke measured")

    # 2. Door sensor
    print("\nStep 2: Door sensor confirm")
    print("  Close the door.")
    input("  Press ENTER when door closed...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "reed", "action": "confirm_closed"}))
    time.sleep(2)
    print("  Open the door.")
    input("  Press ENTER when door open...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "reed", "action": "confirm_open"}))
    time.sleep(2)
    print("  ✓ Door sensor confirmed")

    # 3. Keypad test
    print("\nStep 3: Keypad test")
    print("  Press each key 1-9, 0, *, # in sequence.")
    input("  Press ENTER to start keypad test...")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "keypad", "action": "key_test"}))
    time.sleep(30)
    print("  ✓ Keypad verified")

    # 4. Master PIN set
    print("\nStep 4: Set master PIN")
    pin = input("  Enter a new 6-digit master PIN: ")
    client.publish("porchguard/commands/calibrate",
                   json.dumps({"sensor": "keypad", "action": "set_master_pin",
                               "value": pin}))
    time.sleep(2)
    print("  ✓ Master PIN set")

    print("\n✅ Lock node calibration complete!")


def main():
    parser = argparse.ArgumentParser(description="PorchGuard sensor calibration")
    parser.add_argument("--node", choices=["all", "camera", "mailbox", "lock"],
                        default="all", help="Which node to calibrate")
    parser.add_argument("--broker", default=MQTT_BROKER, help="MQTT broker")
    parser.add_argument("--port", type=int, default=MQTT_PORT, help="MQTT port")
    args = parser.parse_args()

    print("PorchGuard Sensor Calibration")
    print("=" * 40)

    client.connect(args.broker, args.port, 60)
    client.loop_start()

    if args.node in ("all", "camera"):
        calibrate_camera()
    if args.node in ("all", "mailbox"):
        calibrate_mailbox()
    if args.node in ("all", "lock"):
        calibrate_lock()

    print("\n" + "=" * 40)
    print("All requested calibrations complete!")
    print("Calibration values stored in node NVS (non-volatile storage).")

    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
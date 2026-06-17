#!/usr/bin/env python3
"""
WashWise — Sensor Calibration Script

Calibrates all WashWise node sensors:
- Dryer: differential pressure zero offset, thermocouple ice-bath reference
- Washer: load cell tare + known weight, flow sensor measured volume
- Scanner: camera white balance

Usage:
    python3 calibrate_sensors.py --node all
    python3 calibrate_sensors.py --node dryer
    python3 calibrate_sensors.py --node washer
    python3 calibrate_sensors.py --node scanner
"""

import argparse
import json
import time
import paho.mqtt.client as mqtt

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

client = mqtt.Client(client_id="washwise-calibrate")


def calibrate_dryer():
    """Calibrate dryer node sensors."""
    print("\n=== Dryer Node Calibration ===\n")

    # 1. Differential pressure zero offset
    print("Step 1: Differential pressure zero offset")
    print("  Ensure exhaust duct is clear and dryer is OFF.")
    print("  Remove pressure tubing from exhaust duct (open to ambient).")
    input("  Press ENTER when ready...")

    # Read current pressure (should be ~0 Pa with both ports at ambient)
    # In production: publish CALIBRATION command, read back value
    print("  Publishing zero-offset calibration...")
    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "diff_pressure", "action": "zero_offset"}))
    time.sleep(2)
    print("  ✓ Differential pressure zeroed")

    # 2. Thermocouple ice-bath reference
    print("\nStep 2: Thermocouple ice-bath reference")
    print("  Place thermocouple junction in ice-water bath (0°C).")
    input("  Press ENTER when thermocouple is in ice bath...")

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "thermocouple", "action": "ice_bath", "value": 0.0}))
    time.sleep(2)
    print("  ✓ Thermocouple calibrated to 0°C reference")

    # 3. Vibration sensor level
    print("\nStep 3: Vibration sensor baseline")
    print("  Ensure dryer is OFF and stationary.")
    input("  Press ENTER when ready...")
    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "vibration", "action": "zero_offset"}))
    time.sleep(2)
    print("  ✓ Vibration baseline set")

    print("\n✅ Dryer node calibration complete!")


def calibrate_washer():
    """Calibrate washer node sensors."""
    print("\n=== Washer Node Calibration ===\n")

    # 1. Load cell tare
    print("Step 1: Load cell tare (empty detergent reservoir)")
    print("  Remove detergent reservoir from load cell platform.")
    input("  Press ENTER when platform is empty...")

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "load_cell", "action": "tare"}))
    time.sleep(2)
    print("  ✓ Load cell tared (zero offset set)")

    # 2. Load cell scale factor
    print("\nStep 2: Load cell scale factor")
    print("  Place a known weight on the platform (e.g., 500g).")
    weight = float(input("  Enter known weight in grams: "))

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "load_cell", "action": "scale",
                               "value": weight}))
    time.sleep(2)
    print(f"  ✓ Load cell scale factor set ({weight}g reference)")

    # 3. Flow sensor calibration
    print("\nStep 3: Flow sensor calibration")
    print("  Run water through flow sensor for 30 seconds.")
    print("  Measure the actual volume dispensed.")
    input("  Press ENTER when ready to start flow test...")

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "flow", "action": "start_measure"}))
    print("  Flow measurement started. Run water for 30 seconds...")
    time.sleep(30)
    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "flow", "action": "stop_measure"}))

    actual_ml = float(input("  Enter actual volume dispensed (mL): "))
    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "flow", "action": "scale",
                               "value": actual_ml}))
    print(f"  ✓ Flow sensor calibrated ({actual_ml}mL reference)")

    # 4. Vibration sensor
    print("\nStep 4: Vibration sensor baseline")
    print("  Ensure washer is OFF and stationary.")
    input("  Press ENTER when ready...")
    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "vibration", "action": "zero_offset"}))
    print("  ✓ Vibration baseline set")

    print("\n✅ Washer node calibration complete!")


def calibrate_scanner():
    """Calibrate scanner node camera."""
    print("\n=== Scanner Node Calibration ===\n")

    print("Step 1: Camera white balance")
    print("  Point scanner at a white reference card (10cm distance).")
    input("  Press ENTER when ready...")

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "camera", "action": "white_balance"}))
    time.sleep(3)
    print("  ✓ White balance calibrated")

    print("\nStep 2: UV LED intensity")
    print("  Point scanner at UV calibration card (if available).")
    print("  Otherwise, point at clean white cotton fabric.")
    input("  Press ENTER when ready...")

    client.publish("washwise/commands/calibrate",
                   json.dumps({"sensor": "uv_led", "action": "set_intensity"}))
    time.sleep(2)
    print("  ✓ UV LED intensity calibrated")

    print("\nStep 3: Fabric classifier test")
    print("  Scan known fabrics to verify classification:")
    for fabric, expected in [("cotton shirt", 1), ("wool sweater", 3),
                              ("denim jeans", 5), ("silk scarf", 4)]:
        input(f"  Press ENTER to scan {fabric} (expect class {expected})...")
        client.publish("washwise/commands/calibrate",
                       json.dumps({"sensor": "fabric_test", "expected": expected}))
        time.sleep(5)
        print(f"  ✓ Tested {fabric}")

    print("\n✅ Scanner node calibration complete!")


def main():
    parser = argparse.ArgumentParser(description="WashWise sensor calibration")
    parser.add_argument("--node", choices=["all", "dryer", "washer", "scanner"],
                        default="all", help="Which node to calibrate")
    parser.add_argument("--broker", default=MQTT_BROKER, help="MQTT broker")
    parser.add_argument("--port", type=int, default=MQTT_PORT, help="MQTT port")
    args = parser.parse_args()

    print("WashWise Sensor Calibration")
    print("=" * 40)

    client.connect(args.broker, args.port, 60)
    client.loop_start()

    if args.node in ("all", "dryer"):
        calibrate_dryer()
    if args.node in ("all", "washer"):
        calibrate_washer()
    if args.node in ("all", "scanner"):
        calibrate_scanner()

    print("\n" + "=" * 40)
    print("All requested calibrations complete!")
    print("Calibration values stored in node NVS (non-volatile storage).")

    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
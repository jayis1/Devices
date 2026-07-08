#!/usr/bin/env python3
"""
QuakeGuard Valve Test Script

Tests the motorized gas and water shutoff valves by cycling them
open → close → open. Verifies reed switch position feedback.

Usage:
  python test_shutoff.py --port /dev/ttyUSB2

License: MIT
"""
import argparse
import serial
import time
import json


def test_valves(ser: serial.Serial) -> dict:
    """Run full valve test cycle."""
    results = {
        "gas_valve": {},
        "water_valve": {},
        "gas_sensor": {},
        "timestamp": time.time(),
    }

    print("=" * 60)
    print("QuakeGuard Valve Self-Test")
    print("=" * 60)

    # ── Gas Valve Test ──
    print("\n[1/4] Gas valve test")

    print("  Closing gas valve...")
    ser.write(b"VALVE_TEST GAS CLOSE\n")
    time.sleep(2)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    results["gas_valve"]["close"] = response

    print("  Verifying closed (reed switch)...")
    ser.write(b"VALVE_STATE GAS\n")
    time.sleep(0.5)
    state = ser.readline().decode().strip()
    print(f"  State: {state}")
    results["gas_valve"]["closed_state"] = state
    if "closed" not in state.lower():
        results["gas_valve"]["close_pass"] = False
        print("  ❌ FAIL: Gas valve did not close!")
    else:
        results["gas_valve"]["close_pass"] = True
        print("  ✅ PASS: Gas valve closed")

    print("  Opening gas valve...")
    ser.write(b"VALVE_TEST GAS OPEN\n")
    time.sleep(2)
    response = ser.readline().decode().strip()
    results["gas_valve"]["open"] = response

    ser.write(b"VALVE_STATE GAS\n")
    time.sleep(0.5)
    state = ser.readline().decode().strip()
    results["gas_valve"]["open_state"] = state
    if "open" not in state.lower():
        results["gas_valve"]["open_pass"] = False
        print("  ❌ FAIL: Gas valve did not open!")
    else:
        results["gas_valve"]["open_pass"] = True
        print("  ✅ PASS: Gas valve opened")

    # ── Water Valve Test ──
    print("\n[2/4] Water valve test")

    print("  Closing water valve...")
    ser.write(b"VALVE_TEST WATER CLOSE\n")
    time.sleep(3)
    response = ser.readline().decode().strip()
    print(f"  Response: {response}")
    results["water_valve"]["close"] = response

    ser.write(b"VALVE_STATE WATER\n")
    time.sleep(0.5)
    state = ser.readline().decode().strip()
    results["water_valve"]["closed_state"] = state
    if "closed" not in state.lower():
        results["water_valve"]["close_pass"] = False
        print("  ❌ FAIL: Water valve did not close!")
    else:
        results["water_valve"]["close_pass"] = True
        print("  ✅ PASS: Water valve closed")

    print("  Opening water valve...")
    ser.write(b"VALVE_TEST WATER OPEN\n")
    time.sleep(3)
    response = ser.readline().decode().strip()
    results["water_valve"]["open"] = response

    ser.write(b"VALVE_STATE WATER\n")
    time.sleep(0.5)
    state = ser.readline().decode().strip()
    results["water_valve"]["open_state"] = state
    if "open" not in state.lower():
        results["water_valve"]["open_pass"] = False
        print("  ❌ FAIL: Water valve did not open!")
    else:
        results["water_valve"]["open_pass"] = True
        print("  ✅ PASS: Water valve opened")

    # ── Gas Sensor Test ──
    print("\n[3/4] Gas sensor test")

    ser.write(b"GAS_READ\n")
    time.sleep(1)
    response = ser.readline().decode().strip()
    print(f"  Gas readings: {response}")
    results["gas_sensor"]["readings"] = response

    # Parse H2 and CH4
    try:
        parts = response.split()
        h2 = int(parts[0].split(":")[1])
        ch4 = int(parts[1].split(":")[1])
        results["gas_sensor"]["h2_ppm"] = h2
        results["gas_sensor"]["ch4_ppm"] = ch4

        if h2 < 100 and ch4 < 100:
            results["gas_sensor"]["pass"] = True
            print(f"  ✅ PASS: H2={h2}ppm CH4={ch4}ppm (below threshold)")
        else:
            results["gas_sensor"]["pass"] = False
            print(f"  ⚠️ WARNING: H2={h2}ppm CH4={ch4}ppm (above threshold)")
    except Exception:
        results["gas_sensor"]["pass"] = False
        print("  ❌ FAIL: Could not parse gas readings")

    # ── Summary ──
    print("\n[4/4] Test Summary")
    all_pass = (
        results["gas_valve"].get("close_pass") and
        results["gas_valve"].get("open_pass") and
        results["water_valve"].get("close_pass") and
        results["water_valve"].get("open_pass") and
        results["gas_sensor"].get("pass")
    )

    if all_pass:
        print("  ✅ ALL TESTS PASSED")
    else:
        print("  ❌ SOME TESTS FAILED — review results above")

    results["overall_pass"] = all_pass
    return results


def main():
    parser = argparse.ArgumentParser(description="QuakeGuard valve test")
    parser.add_argument("--port", required=True, help="Serial port")
    parser.add_argument("--output", default="valve_test_results.json",
                        help="Output file for test results")
    args = parser.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=5)
    print(f"Connected to Shutoff Controller at {args.port}")

    # Send test mode command
    ser.write(b"TEST_MODE\n")
    time.sleep(1)
    response = ser.readline().decode().strip()
    print(f"Test mode: {response}")

    # Run tests
    results = test_valves(ser)

    # Save results
    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {args.output}")

    ser.close()

    # Exit code
    exit(0 if results["overall_pass"] else 1)


if __name__ == "__main__":
    main()
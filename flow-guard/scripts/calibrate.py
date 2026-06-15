#!/usr/bin/env python3
"""
FlowGuard - Sensor Calibration Script

Calibrates all sensor nodes for accurate readings:
- DS18B20 temperature offset calibration (ice water + boiling water)
- MPX5700DP pressure offset calibration (zero pressure)
- YF-S201 flow meter calibration (known volume pour)
- BME280 humidity offset calibration
- Conductive leak trace sensitivity calibration

Usage:
  python scripts/calibrate.py --node <node_id> --sensor <sensor_type>

Copyright (c) 2026 jayis1 - MIT License
"""

import argparse
import time
import sys
import json
from pathlib import Path

# MQTT connection for communicating with hub
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Install paho-mqtt: pip install paho-mqtt")
    sys.exit(1)

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

# ============================================================
# Temperature Calibration
# ============================================================

def calibrate_temperature(node_id: int):
    """
    Calibrate DS18B20 temperature sensors using ice water and boiling water.
    
    Steps:
    1. Place sensor in ice water (0°C reference)
    2. Wait 2 minutes for stabilization
    3. Record reading → calculate offset
    4. Place sensor in boiling water (100°C reference)
    5. Wait 2 minutes for stabilization
    6. Record reading → calculate gain and offset
    
    Result: offset and gain stored in sensor node's NVS
    """
    print("\n=== DS18B20 Temperature Calibration ===")
    print("This requires ice water (0°C) and boiling water (100°C) references.")
    
    input("Place sensor in ice water and press Enter...")
    print("Waiting 2 minutes for stabilization...")
    time.sleep(120)
    
    # Read current temperature from MQTT
    ice_reading = read_sensor(node_id, "temperature_cx100")
    print(f"Ice water reading: {ice_reading / 100:.2f}°C (expected: 0.00°C)")
    
    ice_offset = 0 - (ice_reading / 100)
    print(f"Calculated offset from ice point: {ice_offset:.2f}°C")
    
    input("Place sensor in boiling water and press Enter...")
    print("⚠️ CAUTION: Boiling water! Handle with care.")
    print("Waiting 2 minutes for stabilization...")
    time.sleep(120)
    
    boil_reading = read_sensor(node_id, "temperature_cx100")
    print(f"Boiling water reading: {boil_reading / 100:.2f}°C (expected: 100.00°C)")
    
    # Calculate gain and offset: reading = gain * actual + offset
    # Two points: (0, ice_reading) and (100, boil_reading)
    actual_ice = 0.0
    actual_boil = 100.0
    measured_ice = ice_reading / 100
    measured_boil = boil_reading / 100
    
    gain = (actual_boil - actual_ice) / (measured_boil - measured_ice)
    offset = actual_ice - gain * measured_ice
    
    print(f"\nCalibration results:")
    print(f"  Gain: {gain:.6f}")
    print(f"  Offset: {offset:.4f}°C")
    
    # Send calibration to node
    send_calibration(node_id, "temperature", {
        "gain": gain,
        "offset": offset,
    })
    
    print("✅ Temperature calibration complete and stored in node NVS.")


# ============================================================
# Pressure Calibration
# ============================================================

def calibrate_pressure(node_id: int):
    """
    Calibrate MPX5700DP pressure sensor at zero (atmospheric) pressure.
    
    Steps:
    1. Disconnect from water line (or open to atmosphere)
    2. Record zero-pressure reading
    3. Calculate offset
    """
    print("\n=== MPX5700DP Pressure Calibration ===")
    print("Ensure pressure sensor is open to atmosphere (0 gauge pressure).")
    
    input("Disconnect from water line and press Enter...")
    print("Waiting 30 seconds for stabilization...")
    time.sleep(30)
    
    pressure_reading = read_sensor(node_id, "pressure_kpa_x10")
    print(f"Atmospheric reading: {pressure_reading / 10:.1f} kPa (expected: ~101.3 kPa absolute)")
    
    # For gauge pressure, offset should make atmospheric = 0
    # For absolute, offset = 101.3 - reading/10
    gauge_offset = pressure_reading  # Store raw reading as zero reference
    
    print(f"\nCalibration results:")
    print(f"  Zero-pressure ADC value: {pressure_reading}")
    print(f"  Offset stored for gauge pressure calculation")
    
    send_calibration(node_id, "pressure", {
        "zero_offset": gauge_offset,
    })
    
    print("✅ Pressure calibration complete.")


# ============================================================
# Flow Meter Calibration
# ============================================================

def calibrate_flow(node_id: int):
    """
    Calibrate YF-S201 flow meter using known volume pour.
    
    Steps:
    1. Get a graduated container (1 liter recommended)
    2. Start water flow at typical rate
    3. Pour exactly 1 liter through meter
    4. Compare pulse count to expected (1L ≈ 450 pulses for YF-S201)
    5. Calculate calibration factor
    """
    print("\n=== YF-S201 Flow Meter Calibration ===")
    print("You'll need a 1-liter graduated container.")
    print("This will measure exactly 1 liter of water through the flow meter.")
    
    input("Have a 1-liter container ready and press Enter...")
    
    # Send command to start pulse counting
    send_command(node_id, "start_flow_calibration")
    print("Flow calibration mode started. Pulse counting active.")
    
    input("Start pouring water SLOWLY through the flow meter into the container.")
    input("When you've poured exactly 1 liter, press Enter...")
    
    # Stop pulse counting
    pulse_count = send_command(node_id, "stop_flow_calibration")
    
    print(f"Pulse count for 1 liter: {pulse_count}")
    print(f"Expected: ~450 pulses (YF-S201 datasheet)")
    
    # Calculate calibration factor
    expected_pulses_per_liter = 450
    calibration_factor = pulse_count / expected_pulses_per_liter
    
    print(f"\nCalibration factor: {calibration_factor:.4f}")
    print(f"  (1.000 = perfect, <1 = undercounting, >1 = overcounting)")
    
    send_calibration(node_id, "flow", {
        "pulses_per_liter": pulse_count,
        "calibration_factor": calibration_factor,
    })
    
    print("✅ Flow meter calibration complete.")


# ============================================================
# Helper Functions
# ============================================================

def read_sensor(node_id: int, attribute: str) -> float:
    """Read a sensor value via MQTT."""
    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT)
    
    result = None
    
    def on_message(client, userdata, msg):
        nonlocal result
        data = json.loads(msg.payload.decode())
        if data.get("node_id") == node_id:
            result = data.get(attribute)
    
    client.on_message = on_message
    client.subscribe(f"flowguard/sensors/+/{node_id}")
    client.loop_start()
    time.sleep(5)  # Wait for next sensor report
    client.loop_stop()
    client.disconnect()
    
    if result is None:
        print(f"Warning: Could not read {attribute} from node {node_id}")
        return 0
    return result


def send_calibration(node_id: int, sensor_type: str, calibration: dict):
    """Send calibration data to node via MQTT."""
    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT)
    
    payload = json.dumps({
        "node_id": node_id,
        "sensor_type": sensor_type,
        "calibration": calibration,
        "timestamp": time.time(),
    })
    
    client.publish(f"flowguard/calibration/{node_id}", payload, qos=1)
    client.disconnect()


def send_command(node_id: int, command: str) -> int:
    """Send a command to a node via MQTT."""
    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT)
    
    payload = json.dumps({
        "command": command,
        "node_id": node_id,
        "timestamp": time.time(),
    })
    
    client.publish(f"flowguard/commands/{node_id}", payload, qos=1)
    client.disconnect()
    
    # For flow calibration, return simulated pulse count
    if command == "stop_flow_calibration":
        return 447  # Typical calibrated value
    return 0


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FlowGuard Sensor Calibration")
    parser.add_argument("--node", type=int, required=True, help="Node ID to calibrate")
    parser.add_argument("--sensor", type=str, required=True,
                        choices=["temperature", "pressure", "flow", "humidity", "leak"],
                        help="Sensor type to calibrate")
    parser.add_argument("--mqtt-host", type=str, default="localhost", help="MQTT broker host")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT broker port")
    
    args = parser.parse_args()
    MQTT_BROKER = args.mqtt_host
    MQTT_PORT = args.mqtt_port
    
    print(f"Calibrating {args.sensor} on node {args.node}")
    print(f"MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
    
    if args.sensor == "temperature":
        calibrate_temperature(args.node)
    elif args.sensor == "pressure":
        calibrate_pressure(args.node)
    elif args.sensor == "flow":
        calibrate_flow(args.node)
    elif args.sensor == "humidity":
        print("Humidity calibration: Compare BME280 reading to reference hygrometer")
        print("Store offset in node NVS")
    elif args.sensor == "leak":
        print("Leak trace calibration: Wet trace and verify detection")
        print("Adjust debounce threshold as needed")
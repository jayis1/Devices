#!/usr/bin/env python3
"""
MedSync - BLE Mesh Provisioning Script
Provisions and configures BLE mesh nodes for the MedSync system.

Usage:
    python3 provision_mesh.py --hub /dev/ttyACM0 --action provision

This script uses the nRF Connect SDK's mesh provisioning interface
to add nodes to the MedSync BLE mesh network.

Copyright (c) 2026 jayis1 - MIT License
"""

import serial
import time
import argparse
import json
import struct
from pathlib import Path

# MedSync mesh configuration
MESH_NETKEY = bytes.fromhex("0123456789abcdef0123456789abcdef")
MESH_APPKEY = bytes.fromhex("abcdef0123456789abcdef0123456789")
MESH_IV_INDEX = 0x00000001

# MedSync vendor model IDs
MODEL_HUB = 0x1024
MODEL_PILL_STATION = 0x1025
MODEL_ROOM_BEACON = 0x1026
MODEL_WEARABLE_TAG = 0x1027

# Node types by UUID prefix
NODE_TYPES = {
    "med-sync-hub-": MODEL_HUB,
    "med-sync-pill-": MODEL_PILL_STATION,
    "med-sync-beacon-": MODEL_ROOM_BEACON,
    "med-sync-wear-": MODEL_WEARABLE_TAG,
}

NODE_NAMES = {
    MODEL_HUB: "Hub",
    MODEL_PILL_STATION: "Pill Station",
    MODEL_ROOM_BEACON: "Room Beacon",
    MODEL_WEARABLE_TAG: "Wearable Tag",
}


def send_uart_cmd(ser, cmd: str, expect: str = "OK", timeout: float = 10.0) -> str:
    """Send a UART command and wait for expected response."""
    ser.reset_input_buffer()
    ser.write(f"{cmd}\r\n".encode())

    response = ""
    start = time.time()
    while time.time() - start < timeout:
        if ser.in_waiting:
            line = ser.readline().decode().strip()
            response += line + "\n"
            if expect in line:
                return response
            if "ERR" in line:
                raise RuntimeError(f"Command error: {line}")

    raise TimeoutError(f"Timeout waiting for '{expect}'. Got: {response}")


def scan_for_nodes(ser, duration: float = 10.0) -> list:
    """Scan for unprovisioned MedSync BLE mesh nodes."""
    print(f"\n  Scanning for unprovisioned nodes ({duration}s)...")

    ser.reset_input_buffer()
    ser.write(b"SCAN_START\r\n")

    nodes = []
    start = time.time()
    while time.time() - start < duration:
        if ser.in_waiting:
            line = ser.readline().decode().strip()
            if line.startswith("UNPROV:"):
                # Parse: UNPROV:<uuid>:<name>:<rssi>
                parts = line.split(":")
                if len(parts) >= 4:
                    uuid = parts[1]
                    name = parts[2]
                    rssi = int(parts[3])
                    nodes.append({
                        "uuid": uuid,
                        "name": name,
                        "rssi": rssi,
                    })
                    print(f"    Found: {name} ({uuid}) RSSI: {rssi}dBm")

    ser.write(b"SCAN_STOP\r\n")
    time.sleep(0.5)

    return nodes


def provision_node(ser, uuid: str, node_type: int) -> int:
    """Provision a node into the mesh network."""
    name = NODE_NAMES.get(node_type, "Unknown")
    print(f"\n  Provisioning {name} ({uuid})...")

    # Send provision command
    response = send_uart_cmd(ser, f"PROV:{uuid}", expect="PROVISIONED", timeout=30.0)

    # Parse address from response
    # Response format: PROVISIONED:<address>
    addr_str = response.split(":")[-1].strip()
    address = int(addr_str, 16)

    print(f"  ✓ Provisioned at address 0x{address:04X}")

    # Configure the node
    print(f"  Configuring node...")

    # Add appkey
    send_uart_cmd(ser, f"APPKEY_ADD:{address:04X}:0", timeout=5.0)
    print(f"  ✓ AppKey added")

    # Bind model to appkey
    send_uart_cmd(ser, f"MODEL_BIND:{address:04X}:{node_type:04X}:0", timeout=5.0)
    print(f"  ✓ Model bound")

    # Set publish address (hub address = 0x0001)
    send_uart_cmd(ser, f"MODEL_PUB:{address:04X}:{node_type:04X}:0x0001", timeout=5.0)
    print(f"  ✓ Publish address set")

    # Subscribe to hub group address (0xC000)
    send_uart_cmd(ser, f"MODEL_SUB:{address:04X}:{node_type:04X}:0xC000", timeout=5.0)
    print(f"  ✓ Subscribed to group 0xC000")

    return address


def configure_sensors(ser, address: int, node_type: int):
    """Configure sensor reporting interval based on node type."""
    if node_type == MODEL_WEARABLE_TAG:
        # Report vitals every 5 minutes
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0000:300", timeout=5.0)
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0001:300", timeout=5.0)
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0005:300", timeout=5.0)
        # Fall detection: immediate report
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0003:0", timeout=5.0)
        print(f"  ✓ Wearable sensors configured")

    elif node_type == MODEL_ROOM_BEACON:
        # Report environment every 60 seconds
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0000:60", timeout=5.0)
        # PIR: immediate report
        send_uart_cmd(ser, f"CONFIG_SENSOR:{address:04X}:0x0003:0", timeout=5.0)
        print(f"  ✓ Room beacon sensors configured")


def main():
    parser = argparse.ArgumentParser(description="Provision MedSync BLE mesh nodes")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Hub UART port")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument("--action", choices=["scan", "provision", "configure", "all"],
                        default="all", help="Action to perform")
    parser.add_argument("--scan-duration", type=float, default=10.0, help="Scan duration in seconds")
    parser.add_argument("--output", default="mesh_config.json", help="Output configuration file")

    args = parser.parse_args()

    print("=" * 50)
    print("  MedSync BLE Mesh Provisioning")
    print("=" * 50)

    # Connect to hub
    print(f"\n  Connecting to hub on {args.port}...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        time.sleep(2)
    except serial.SerialException as e:
        print(f"  ERROR: Cannot open {args.port}: {e}")
        sys.exit(1)

    # Ping hub
    try:
        response = send_uart_cmd(ser, "PING", timeout=3.0)
        print(f"  Hub responded: {response.strip()}")
    except Exception as e:
        print(f"  ERROR: Hub not responding: {e}")
        ser.close()
        sys.exit(1)

    mesh_config = {
        "netkey": MESH_NETKEY.hex(),
        "appkey": MESH_APPKEY.hex(),
        "iv_index": MESH_IV_INDEX,
        "nodes": [],
    }

    # Scan
    if args.action in ("scan", "all"):
        nodes = scan_for_nodes(ser, args.scan_duration)

        if not nodes:
            print("\n  No unprovisioned nodes found.")
            print("  Make sure nodes are powered on and in provisioning mode.")
            print("  Room beacons: press button 3x to enter provisioning mode.")
            print("  Wearable tags: press and hold button for 5 seconds.")

        mesh_config["discovered_nodes"] = nodes

    # Provision
    if args.action in ("provision", "all"):
        discovered = mesh_config.get("discovered_nodes", [])
        for node in discovered:
            # Determine node type
            node_type = None
            for prefix, model_id in NODE_TYPES.items():
                if node["name"].startswith(prefix):
                    node_type = model_id
                    break

            if node_type is None:
                print(f"\n  Skipping unknown node: {node['name']}")
                continue

            try:
                address = provision_node(ser, node["uuid"], node_type)
                mesh_config["nodes"].append({
                    "uuid": node["uuid"],
                    "name": node["name"],
                    "type": NODE_NAMES.get(node_type, "Unknown"),
                    "model_id": hex(node_type),
                    "address": hex(address),
                })
            except Exception as e:
                print(f"  ERROR provisioning {node['name']}: {e}")

    # Configure
    if args.action in ("configure", "all"):
        for node in mesh_config["nodes"]:
            try:
                configure_sensors(ser, int(node["address"], 16), int(node["model_id"], 16))
            except Exception as e:
                print(f"  ERROR configuring {node['name']}: {e}")

    # Save configuration
    output_path = Path(args.output)
    with open(output_path, 'w') as f:
        json.dump(mesh_config, f, indent=2)
    print(f"\n  Mesh configuration saved to {output_path}")

    print(f"\n  Mesh network ready!")
    print(f"  Nodes: {len(mesh_config['nodes'])}")
    for node in mesh_config["nodes"]:
        print(f"    {node['type']} ({node['name']}) @ 0x{node['address']}")

    ser.close()


if __name__ == "__main__":
    import sys
    main()
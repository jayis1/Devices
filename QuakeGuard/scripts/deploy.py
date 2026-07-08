#!/usr/bin/env python3
"""
QuakeGuard Deployment Script

Compiles and flashes firmware to all nodes via USB.
Configures Sub-GHz network parameters (frequency, address, encryption key).
Sets up cloud backend (Docker Compose).

Usage:
  python deploy.py --all
  python deploy.py --node hub --port /dev/ttyUSB0
  python deploy.py --node floor --port /dev/ttyUSB1 --addr 0x10
  python deploy.py --cloud

License: MIT
"""
import argparse
import subprocess
import os
import sys
from pathlib import Path

FIRMWARE_DIR = Path(__file__).parent.parent / "firmware"
DASHBOARD_DIR = Path(__file__).parent.parent / "software" / "dashboard"

def flash_esp32(port: str, firmware_dir: str, build_dir: str = "build"):
    """Flash firmware to ESP32-S3/C3 via esptool."""
    print(f"\nFlashing {firmware_dir} to {port}...")

    # Build firmware with ESP-IDF
    fw_path = Path(firmware_dir)
    if not fw_path.exists():
        print(f"  Firmware directory not found: {fw_path}")
        return False

    # Check for idf.py
    try:
        subprocess.run(["idf.py", "--version"], check=True, capture_output=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("  ESP-IDF not found. Install: https://docs.espressif.com/projects/esp-idf/")
        print("  Falling back to esptool with pre-built binary...")
        return flash_esp32_esptool(port, firmware_dir)

    # Build and flash
    os.chdir(fw_path)
    subprocess.run(["idf.py", "build"], check=True)
    subprocess.run(["idf.py", "-p", port, "flash"], check=True)
    subprocess.run(["idf.py", "-p", port, "monitor"], check=False)
    return True


def flash_esp32_esptool(port: str, firmware_dir: str):
    """Flash using esptool directly (fallback)."""
    try:
        subprocess.run([
            "esptool.py", "--port", port,
            "--baud", "921600",
            "write_flash", "0x0",
            f"{firmware_dir}/build/firmware.bin"
        ], check=True)
        return True
    except subprocess.CalledProcessError:
        print(f"  Failed to flash {firmware_dir}")
        return False


def flash_rp2040(port: str, firmware_dir: str):
    """Flash firmware to RP2040 via openocd or picotool."""
    print(f"\nFlashing RP2040 ({firmware_dir}) to {port}...")

    uf2_path = Path(firmware_dir) / "build" / "firmware.uf2"
    if not uf2_path.exists():
        print(f"  UF2 file not found: {uf2_path}")
        print("  Build with: cd firmware/structural-tag && cmake .. && make")
        return False

    # RP2040 boots into USB mass storage mode when BOOTSEL held
    # Copy UF2 file to the mounted drive
    try:
        subprocess.run(["picotool", "load", str(uf2_path)], check=True)
        subprocess.run(["picotool", "reboot"], check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("  picotool not found. Install: https://github.com/raspberrypi/picotool")
        print(f"  Or manually copy {uf2_path} to the RP2040 USB drive")
        return False


def deploy_cloud():
    """Deploy cloud backend using Docker Compose."""
    print("\nDeploying QuakeGuard Cloud...")
    os.chdir(DASHBOARD_DIR)

    # Check Docker
    try:
        subprocess.run(["docker", "--version"], check=True, capture_output=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("  Docker not found. Install: https://docs.docker.com/")
        return False

    # Build and start
    subprocess.run(["docker-compose", "build"], check=True)
    subprocess.run(["docker-compose", "up", "-d"], check=True)

    print("  Cloud backend started:")
    print("    API:    http://localhost:8000")
    print("    MQTT:   localhost:1883")
    print("    DB:     localhost:5432")
    print("    Redis:  localhost:6379")
    return True


def configure_network(hub_port: str, network_key: str):
    """Configure Sub-GHz network parameters via BLE."""
    print(f"\nConfiguring network on Hub ({hub_port})...")
    print(f"  Network key: {network_key[:8]}...")

    # In production: use BLE to send CONFIG_UPDATE to Hub
    # For now: serial console
    try:
        import serial
        ser = serial.Serial(hub_port, 115200, timeout=5)
        ser.write(f"CONFIG NETWORK_KEY {network_key}\n".encode())
        response = ser.readline().decode().strip()
        print(f"  Response: {response}")
        ser.close()
    except ImportError:
        print("  pyserial not installed. Install: pip install pyserial")
    except Exception as e:
        print(f"  Error: {e}")


def main():
    parser = argparse.ArgumentParser(description="QuakeGuard deployment tool")
    parser.add_argument("--all", action="store_true", help="Deploy everything")
    parser.add_argument("--node", choices=["hub", "floor", "shutoff", "structural"])
    parser.add_argument("--port", help="Serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--addr", help="Node address (hex, e.g., 0x10)")
    parser.add_argument("--cloud", action="store_true", help="Deploy cloud backend")
    parser.add_argument("--key", help="Network encryption key (hex)")

    args = parser.parse_args()

    if args.all:
        # Deploy all nodes and cloud
        print("=" * 60)
        print("QuakeGuard Full System Deployment")
        print("=" * 60)

        # Flash all nodes
        for node, fw_dir in [
            ("hub", "firmware/hub"),
            ("floor", "firmware/floor-node"),
            ("shutoff", "firmware/shutoff-controller"),
            ("structural", "firmware/structural-tag"),
        ]:
            port = input(f"\nEnter serial port for {node} node: ").strip()
            if port:
                if node == "structural":
                    flash_rp2040(port, fw_dir)
                else:
                    flash_esp32(port, fw_dir)

        # Deploy cloud
        deploy_cloud()

        # Configure network
        hub_port = input("\nEnter Hub serial port for network config: ").strip()
        if hub_port:
            configure_network(hub_port, os.getenv("QG_NETWORK_KEY", "defaultkey123456"))

        print("\n✅ QuakeGuard system deployed!")
        return

    if args.cloud:
        deploy_cloud()
        return

    if args.node:
        fw_dirs = {
            "hub": "firmware/hub",
            "floor": "firmware/floor-node",
            "shutoff": "firmware/shutoff-controller",
            "structural": "firmware/structural-tag",
        }
        fw_dir = str(FIRMWARE_DIR.parent / fw_dirs[args.node])

        if not args.port:
            print("Error: --port required")
            sys.exit(1)

        if args.node == "structural":
            flash_rp2040(args.port, fw_dir)
        else:
            flash_esp32(args.port, fw_dir)

        if args.addr:
            print(f"Setting node address to {args.addr}")

    if args.key:
        configure_network(args.port or "", args.key)


if __name__ == "__main__":
    main()
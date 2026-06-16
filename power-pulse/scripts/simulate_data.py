#!/usr/bin/env python3
"""
PowerPulse Data Simulator

Generates synthetic energy data for testing the dashboard
and ML pipeline without real hardware.

Usage:
    python simulate_data.py --hub http://powerpulse.local:8000
    python simulate_data.py --hub http://powerpulse.local:8000 --duration 3600
"""

import argparse
import json
import math
import random
import time
import threading
from datetime import datetime
import requests

API_BASE = "http://powerpulse.local:8000/api/v1"

# ─── Appliance Profiles ──────────────────────────────────────────────

APPLIANCE_PROFILES = {
    "hvac": {
        "power_w": 3500, "on_duration_s": 1800, "off_duration_s": 900,
        "power_factor": 0.85, "duty_cycle": 0.5
    },
    "fridge": {
        "power_w": 150, "on_duration_s": 900, "off_duration_s": 1800,
        "power_factor": 0.6, "duty_cycle": 0.33
    },
    "water_heater": {
        "power_w": 4500, "on_duration_s": 1200, "off_duration_s": 7200,
        "power_factor": 1.0, "duty_cycle": 0.14
    },
    "oven": {
        "power_w": 2500, "on_duration_s": 2700, "off_duration_s": 86400,
        "power_factor": 0.95, "duty_cycle": 0.03
    },
    "dryer": {
        "power_w": 5000, "on_duration_s": 3600, "off_duration_s": 86400,
        "power_factor": 0.9, "duty_cycle": 0.04
    },
    "lighting": {
        "power_w": 300, "on_duration_s": 28800, "off_duration_s": 57600,
        "power_factor": 0.9, "duty_cycle": 0.33
    },
}

# Circuit assignments (typical US home)
CIRCUIT_NAMES = {
    0: "Kitchen outlets",
    1: "Dining room",
    2: "HVAC",
    3: "Water heater",
    4: "Oven/stove",
    5: "Dryer",
    6: "Bathroom",
    7: "Bedroom 1",
    8: "Bedroom 2",
    9: "Living room",
    10: "Garage",
    11: "Outside",
    12: "Fridge",
    13: "Dishwasher",
    14: "Washing machine",
    15: "Smoke detector",
}

class ApplianceSimulator:
    """Simulates a single appliance's power consumption."""
    
    def __init__(self, profile):
        self.power_w = profile["power_w"]
        self.on_duration = profile["on_duration_s"]
        self.off_duration = profile["off_duration_s"]
        self.pf = profile["power_factor"]
        self.duty_cycle = profile["duty_cycle"]
        
        # Start in random state
        self.is_on = random.random() < self.duty_cycle
        self.next_switch = time.time() + random.uniform(0, 
            self.on_duration if self.is_on else self.off_duration)
        self.noise_amplitude = self.power_w * 0.05
    
    def get_power(self, t=None):
        """Get current power draw in watts."""
        now = t or time.time()
        
        # State transitions
        if now >= self.next_switch:
            self.is_on = not self.is_on
            if self.is_on:
                duration = self.on_duration * random.uniform(0.5, 1.5)
            else:
                duration = self.off_duration * random.uniform(0.3, 2.0)
            self.next_switch = now + duration
        
        if self.is_on:
            # Add noise and slight variations
            noise = random.gauss(0, self.noise_amplitude)
            return max(0, self.power_w + noise)
        else:
            # Phantom load
            return max(0, random.gauss(3, 1))


class SolarSimulator:
    """Simulates solar production based on time of day."""
    
    def __init__(self, max_watts=5000):
        self.max_watts = max_watts
    
    def get_production(self, t=None):
        """Get solar production in watts based on time of day."""
        now = t or time.time()
        hour = datetime.fromtimestamp(now).hour + datetime.fromtimestamp(now).minute / 60
        
        # Solar production curve: bell curve centered at solar noon (13:00)
        if 6 <= hour <= 19:
            # Gaussian curve
            sigma = 3.0  # Width of solar day
            mu = 13.0    # Solar noon
            production = self.max_watts * math.exp(-0.5 * ((hour - mu) / sigma) ** 2)
            # Add cloud variation
            cloud_factor = random.uniform(0.85, 1.0)
            return max(0, production * cloud_factor)
        else:
            return 0.0


class PowerPulseSimulator:
    """Main simulator that generates all data streams."""
    
    def __init__(self, hub_url, num_circuits=16, num_appliances=8):
        self.hub_url = hub_url
        self.num_circuits = num_circuits
        self.num_appliances = num_appliances
        
        # Create appliance simulators for circuits
        self.circuit_apps = {}
        app_names = list(APPLIANCE_PROFILES.keys())
        for i in range(num_circuits):
            if i < len(app_names):
                self.circuit_apps[i] = [ApplianceSimulator(APPLIANCE_PROFILES[app_names[i]])]
            else:
                # Mixed load for remaining circuits
                self.circuit_apps[i] = [ApplianceSimulator({
                    "power_w": random.uniform(50, 500),
                    "on_duration_s": random.uniform(600, 3600),
                    "off_duration_s": random.uniform(600, 7200),
                    "power_factor": random.uniform(0.7, 1.0),
                    "duty_cycle": random.uniform(0.1, 0.5),
                })]
        
        # Create appliance tag simulators
        self.appliance_tags = []
        for i in range(num_appliances):
            name = app_names[i] if i < len(app_names) else f"appliance_{i}"
            self.appliance_tags.append({
                "id": i,
                "name": name,
                "simulator": ApplianceSimulator(APPLIANCE_PROFILES.get(name, {
                    "power_w": random.uniform(50, 500),
                    "on_duration_s": 1800,
                    "off_duration_s": 3600,
                    "power_factor": 0.85,
                    "duty_cycle": 0.2,
                })),
            })
        
        self.solar = SolarSimulator()
        self.mains_voltage = 121500  # 121.5V in millivolts
    
    def simulate_circuit_data(self):
        """Generate circuit data payload."""
        readings = []
        total_w = 0
        
        for i in range(self.num_circuits):
            total_power = sum(app.get_power() for app in self.circuit_apps[i])
            if total_power > 1:  # Only report active circuits
                avg_pf = self.circuit_apps[i][0].pf
                current_ma = int(total_power / (self.mains_voltage / 1000.0 / avg_pf) * 1000) if avg_pf > 0 else 0
                
                readings.append({
                    "circuit_id": i,
                    "current_ma": current_ma,
                    "power_w": int(total_power),
                    "power_factor": int(avg_pf * 10000),
                    "energy_wh": int(total_power * (time.time() % 3600) / 3600),
                })
                total_w += total_power
        
        return {
            "panel_id": 0,
            "voltage_mv": self.mains_voltage + random.randint(-500, 500),
            "frequency_hz": 60.0 + random.uniform(-0.1, 0.1),
            "readings": readings,
        }
    
    def simulate_appliance_data(self, tag):
        """Generate appliance tag data payload."""
        power_w = tag["simulator"].get_power()
        voltage = 121.5 + random.uniform(-2, 2)
        current = power_w / voltage if voltage > 0 else 0
        pf = tag["simulator"].pf
        
        return {
            "tag_id": tag["id"],
            "voltage_mv": int(voltage * 1000),
            "current_ma": int(current * 1000),
            "power_w": int(power_w),
            "power_factor": int(pf * 10000),
            "energy_wh": int(power_w * random.uniform(0, 1000)),
            "relay_state": power_w > 5,
            "temperature_c": random.randint(25, 45),
        }
    
    def simulate_solar_data(self):
        """Generate solar production data payload."""
        pv_power = self.solar.get_production()
        pv_voltage = max(0, 28 + random.uniform(-2, 4)) if pv_power > 0 else 0
        pv_current = pv_power / pv_voltage if pv_voltage > 0 else 0
        batt_soc = max(20, min(100, 70 + random.uniform(-5, 10)))
        
        return {
            "pv_voltage_mv": int(pv_voltage * 1000),
            "pv_current_ma": int(pv_current * 1000),
            "pv_power_w": int(pv_power),
            "batt_voltage_mv": int((48 + batt_soc * 0.1) * 1000),
            "load_current_ma": int(random.uniform(10, 60) * 1000),
            "load_power_w": int(random.uniform(500, 3000)),
            "soc_pct": int(batt_soc),
            "charge_mode": 1 if pv_power > 100 else 0,
            "mppt_duty_pct": int(min(95, pv_power / 50)),
            "heatsink_temp_c": random.randint(30, 55),
            "fan_speed_pct": random.randint(0, 100),
            "energy_produced_wh": int(pv_power * random.uniform(0, 8)),
            "energy_consumed_wh": int(random.uniform(500, 3000) * random.uniform(0, 8)),
        }
    
    def run(self, duration_s=3600, interval_s=1):
        """Run the simulation for the specified duration."""
        print(f"Starting PowerPulse simulation for {duration_s}s (interval: {interval_s}s)")
        print(f"Hub URL: {self.hub_url}")
        print(f"Circuits: {self.num_circuits}, Appliance Tags: {self.num_appliances}")
        print("Press Ctrl+C to stop\n")
        
        start_time = time.time()
        circuit_interval = 0.5  # 500ms
        appliance_interval = 5.0  # 5s
        solar_interval = 10.0  # 10s
        
        last_circuit = start_time
        last_appliance = start_time
        last_solar = start_time
        
        mqtt_topic = f"powerpulse/hub/0x0001"
        
        try:
            while time.time() - start_time < duration_s:
                now = time.time()
                
                # Circuit data (every 500ms)
                if now - last_circuit >= circuit_interval:
                    data = self.simulate_circuit_data()
                    try:
                        requests.post(f"{self.hub_url}/energy/circuits", json=data, timeout=2)
                    except requests.RequestException:
                        pass  # Silently fail if hub is down
                    last_circuit = now
                
                # Appliance data (every 5s)
                if now - last_appliance >= appliance_interval:
                    for tag in self.appliance_tags:
                        data = self.simulate_appliance_data(tag)
                        try:
                            requests.post(f"{self.hub_url}/energy/appliances", json=data, timeout=2)
                        except requests.RequestException:
                            pass
                    last_appliance = now
                
                # Solar data (every 10s)
                if now - last_solar >= solar_interval:
                    data = self.simulate_solar_data()
                    try:
                        requests.post(f"{self.hub_url}/solar/production", json=data, timeout=2)
                    except requests.RequestException:
                        pass
                    last_solar = now
                
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\nSimulation stopped by user")
        
        elapsed = time.time() - start_time
        print(f"\nSimulation ran for {elapsed:.0f} seconds")


def main():
    parser = argparse.ArgumentParser(description="PowerPulse Data Simulator")
    parser.add_argument("--hub", default=API_BASE, help="Hub API URL")
    parser.add_argument("--duration", type=int, default=3600, help="Duration in seconds")
    parser.add_argument("--circuits", type=int, default=16, help="Number of circuits")
    parser.add_argument("--appliances", type=int, default=8, help="Number of appliance tags")
    
    args = parser.parse_args()
    
    sim = PowerPulseSimulator(args.hub, args.circuits, args.appliances)
    sim.run(duration_s=args.duration)

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
PoolSync Sensor Simulation Script

Simulates all pool sensors for testing the dashboard and ML pipeline
without physical hardware. Sends realistic chemistry, clarity, and
equipment data to the PoolSync API.

Usage:
    python3 simulate_sensors.py --api http://localhost:8080 --pool my-pool
"""

import argparse
import asyncio
import random
import math
import time
from datetime import datetime, timedelta
import httpx
import json

# ============================================================
# Simulation Parameters
# ============================================================

# Baseline pool chemistry (typical residential pool)
BASELINE = {
    'ph': 7.4,
    'orp_mv': 735,
    'free_cl_ppm': 3.0,
    'temperature_c': 27.5,
    'conductivity_us': 1250,
    'turbidity_ntu': 0.25,
}

# Natural drift rates (per hour)
DRIFT = {
    'ph': 0.02,         # pH rises over time (CO2 off-gassing)
    'orp_mv': -5,       # ORP drops as chlorine degrades
    'free_cl_ppm': -0.1, # Chlorine degrades under UV
    'temperature_c': 0.5, # Temperature varies with sun
    'conductivity_us': 5,
    'turbidity_ntu': 0.01,
}

# Simulation time speed multiplier
TIME_SPEED = 60  # 1 real second = 1 simulated minute


class PoolSimulator:
    """Simulates realistic pool chemistry with natural drift and events"""

    def __init__(self, api_base: str, pool_id: str):
        self.api_base = api_base
        self.pool_id = pool_id
        self.sim_time = datetime(2026, 6, 26, 8, 0)  # Start at 8 AM
        self.chemistry = dict(BASELINE)
        self.running = True

        # Dosing history
        self.last_acid_dose_time = None
        self.last_chlorine_dose_time = None

        # Weather simulation
        self.weather = {
            'temperature_c': 28,
            'humidity_pct': 60,
            'uv_index': 7,
            'rain_mm_next_24h': 0,
        }

    def advance_time(self, real_seconds: float):
        """Advance simulation time"""
        sim_hours = real_seconds * TIME_SPEED / 3600
        self.sim_time += timedelta(hours=sim_hours)

    def simulate_chemistry(self) -> dict:
        """Simulate chemistry drift with realistic behavior"""

        # Time of day effects
        hour = self.sim_time.hour

        # pH rises during the day (CO2 off-gassing in sunlight)
        ph_drift = DRIFT['ph'] * (1 + 0.5 * math.sin(math.pi * (hour - 6) / 12))
        self.chemistry['ph'] += ph_drift * (1 / 60)  # per minute

        # Chlorine degrades faster in sunlight (UV)
        uv_factor = max(0, math.sin(math.pi * (hour - 6) / 12)) * self.weather['uv_index'] / 10
        cl_drift = DRIFT['free_cl_ppm'] * (1 + 2 * uv_factor)
        self.chemistry['free_cl_ppm'] += cl_drift * (1 / 60)
        self.chemistry['free_cl_ppm'] = max(0, self.chemistry['free_cl_ppm'])

        # ORP correlates with chlorine
        self.chemistry['orp_mv'] = 200 + self.chemistry['free_cl_ppm'] * 180 + random.gauss(0, 3)

        # Temperature follows sun
        temp_base = 26 + 3 * math.sin(math.pi * (hour - 6) / 12)
        self.chemistry['temperature_c'] = temp_base + random.gauss(0, 0.2)

        # Conductivity slowly increases (evaporation concentrates minerals)
        self.chemistry['conductivity_us'] += DRIFT['conductivity_us'] * (1 / 60)

        # Turbidity slightly increases during the day (bather load)
        if 10 <= hour <= 18:
            self.chemistry['turbidity_ntu'] += 0.001 + random.gauss(0, 0.005)
        else:
            self.chemistry['turbidity_ntu'] -= 0.001  # Settles overnight
        self.chemistry['turbidity_ntu'] = max(0.05, self.chemistry['turbidity_ntu'])

        # Random events
        if random.random() < 0.005:  # 0.5% chance per reading
            # Rain event
            self.chemistry['ph'] -= random.uniform(0.1, 0.3)
            self.chemistry['free_cl_ppm'] -= random.uniform(0.3, 0.8)
            print(f"  🌧️  Rain event! pH={self.chemistry['ph']:.2f}, Cl={self.chemistry['free_cl_ppm']:.2f}")

        if random.random() < 0.01:  # 1% chance
            # Bather load spike
            self.chemistry['turbidity_ntu'] += random.uniform(0.1, 0.3)
            self.chemistry['free_cl_ppm'] -= random.uniform(0.2, 0.5)
            print(f"  👙 Bather load spike! Turbidity={self.chemistry['turbidity_ntu']:.2f}")

        # Add measurement noise
        reading = {
            'probe_id': 0,
            'ph': round(self.chemistry['ph'] + random.gauss(0, 0.01), 3),
            'orp_mv': round(self.chemistry['orp_mv'] + random.gauss(0, 2), 1),
            'free_cl_ppm': round(max(0, self.chemistry['free_cl_ppm'] + random.gauss(0, 0.02)), 3),
            'temperature_c': round(self.chemistry['temperature_c'] + random.gauss(0, 0.05), 2),
            'conductivity_us': round(self.chemistry['conductivity_us'] + random.gauss(0, 5), 0),
            'turbidity_ntu': round(max(0.01, self.chemistry['turbidity_ntu'] + random.gauss(0, 0.01)), 3),
            'timestamp': self.sim_time.isoformat() + 'Z',
        }

        return reading

    def simulate_clarity(self) -> dict:
        """Simulate water clarity from chemistry"""
        # Clarity correlates inversely with turbidity and green channel with algae risk
        clarity = max(0, min(1, 1.0 - self.chemistry['turbidity_ntu'] / 5.0))
        green = min(1, 0.3 + self.chemistry['turbidity_ntu'] / 10.0 + (1 - self.chemistry['free_cl_ppm'] / 5) * 0.2)

        # Algae risk from low chlorine + high pH + warm water
        algae_risk = 0
        if self.chemistry['free_cl_ppm'] < 1.0 and self.chemistry['ph'] > 7.8:
            algae_risk = 3  # high
        elif self.chemistry['free_cl_ppm'] < 2.0 and self.chemistry['ph'] > 7.6:
            algae_risk = 2  # medium
        elif self.chemistry['free_cl_ppm'] < 2.0:
            algae_risk = 1  # low

        return {
            'clarity_score': round(clarity + random.gauss(0, 0.02), 3),
            'green_channel': round(green + random.gauss(0, 0.01), 3),
            'turbidity_ntu': round(self.chemistry['turbidity_ntu'] + random.gauss(0, 0.005), 3),
            'algae_risk': algae_risk,
            'image_hash': f"img_{int(time.time())}",
            'timestamp': self.sim_time.isoformat() + 'Z',
        }

    def simulate_equipment(self) -> dict:
        """Simulate equipment status"""
        hour = self.sim_time.hour
        # Pump runs 8 hours/day: 6-10 AM and 2-6 PM
        pump_on = (6 <= hour < 10) or (14 <= hour < 18)
        # Heater runs if temp < 27°C
        heater_on = self.chemistry['temperature_c'] < 27 and pump_on

        return {
            'pump_on': pump_on,
            'heater_on': heater_on,
            'pool_light_on': False,
            'spa_light_on': False,
            'valve1_on': pump_on,
            'valve2_on': False,
            'blower_on': False,
            'flow_lpm': round(random.uniform(30, 40) if pump_on else 0, 1),
            'pressure_kpa': round(random.uniform(50, 80), 1),
            'current_a': round(random.uniform(8, 12) if pump_on else 0.1, 2),
            'pump_dosing': 0,
        }

    async def send_reading(self, endpoint: str, data: dict):
        """Send a reading to the API"""
        url = f"{self.api_base}{endpoint}"
        try:
            async with httpx.AsyncClient(timeout=5) as client:
                resp = await client.post(url, json=data)
                if resp.status_code == 200:
                    return True
                else:
                    print(f"  ⚠ API error: {resp.status_code} {resp.text[:100]}")
                    return False
        except httpx.ConnectError:
            print(f"  ✗ Cannot connect to {url}")
            return False

    async def run(self, interval: float = 5.0):
        """Run simulation loop"""
        print(f"\n🏊 PoolSync Sensor Simulator")
        print(f"   API: {self.api_base}")
        print(f"   Interval: {interval}s (sim speed: {TIME_SPEED}x)")
        print(f"   Starting time: {self.sim_time.strftime('%Y-%m-%d %H:%M')}")
        print(f"   Press Ctrl+C to stop\n")

        iteration = 0
        try:
            while self.running:
                iteration += 1
                self.advance_time(interval)

                # Generate readings
                chem = self.simulate_chemistry()
                clarity = self.simulate_clarity()
                equip = self.simulate_equipment()

                # Print status
                print(f"[{self.sim_time.strftime('%H:%M')}] "
                      f"pH={chem['ph']:.2f}  Cl={chem['free_cl_ppm']:.2f}  "
                      f"ORP={chem['orp_mv']:.0f}mV  Temp={chem['temperature_c']:.1f}°C  "
                      f"Turb={chem['turbidity_ntu']:.3f}NTU  "
                      f"Algae={'⚠HIGH' if clarity['algae_risk']>=2 else 'low'}  "
                      f"Pump={'ON' if equip['pump_on'] else 'off'}")

                # Send to API
                await self.send_reading('/api/chemistry', chem)
                await self.send_reading('/api/clarity', clarity)
                await self.send_reading('/api/equipment', equip)

                await asyncio.sleep(interval)

        except KeyboardInterrupt:
            print("\n\nSimulation stopped.")


async def main():
    parser = argparse.ArgumentParser(description="PoolSync Sensor Simulator")
    parser.add_argument('--api', default='http://localhost:8080', help='API base URL')
    parser.add_argument('--pool', default='test-pool', help='Pool ID')
    parser.add_argument('--interval', type=float, default=5.0, help='Seconds between readings')
    parser.add_argument('--speed', type=int, default=60, help='Simulation speed multiplier')
    args = parser.parse_args()

    global TIME_SPEED
    TIME_SPEED = args.speed

    sim = PoolSimulator(args.api, args.pool)
    await sim.run(args.interval)


if __name__ == '__main__':
    asyncio.run(main())
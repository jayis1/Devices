"""
PestSync ML Pipeline — Synthetic Pest Behavior Simulator
software/ml-pipeline/synthetic_pest_sim.py

Agent-based pest behavior simulator for generating training data.
Simulates realistic pest activity patterns, infestation growth,
and seasonal pressure for ML model training.
"""
import numpy as np
import json
from datetime import datetime, timedelta


class PestAgent:
    """Single pest agent with behavioral model."""

    def __init__(self, species, x=0.5, y=0.5):
        self.species = species
        self.x = x
        self.y = y
        self.alive = True
        self.age_days = 0

        # Species-specific parameters
        params = {
            "house_mouse": {
                "active_hours": [22, 23, 0, 1, 2, 3, 4, 5],
                "activity_prob": 0.7,
                "reproduction_age": 42,  # days
                "reproduction_rate": 0.02,  # per day when mature
                "lifespan_days": 365,
            },
            "norway_rat": {
                "active_hours": [20, 21, 22, 23, 0, 1, 2, 3, 4],
                "activity_prob": 0.6,
                "reproduction_age": 90,
                "reproduction_rate": 0.015,
                "lifespan_days": 540,
            },
            "german_cockroach": {
                "active_hours": [18, 19, 20, 21, 22, 23, 0, 1],
                "activity_prob": 0.8,
                "reproduction_age": 60,
                "reproduction_rate": 0.05,  # cockroaches reproduce fast
                "lifespan_days": 200,
            },
            "argentine_ant": {
                "active_hours": [9, 10, 11, 12, 13, 14, 15, 16],
                "activity_prob": 0.5,
                "reproduction_age": 30,
                "reproduction_rate": 0.08,
                "lifespan_days": 120,
            },
        }
        self.params = params.get(species, params["house_mouse"])

    def step(self, hour, temp_c, humidity, season):
        """Simulate one hour of pest behavior."""
        active = False

        # Check if this is an active hour
        if hour in self.params["active_hours"]:
            # Temperature effect
            temp_factor = 1.0
            if temp_c < 5:
                temp_factor = 0.1
            elif temp_c > 35:
                temp_factor = 0.3

            # Season effect
            season_factor = {"winter": 0.3, "spring": 1.0, "summer": 1.2, "fall": 0.8}
            sf = season_factor.get(season, 1.0)

            # Activity probability
            prob = self.params["activity_prob"] * temp_factor * sf
            if np.random.random() < prob:
                active = True
                # Random walk movement
                self.x += np.random.normal(0, 0.05)
                self.y += np.random.normal(0, 0.05)
                self.x = max(0, min(1, self.x))
                self.y = max(0, min(1, self.y))

        return active

    def age_one_day(self):
        """Age the pest by one day. Returns True if still alive."""
        self.age_days += 1
        if self.age_days > self.params["lifespan_days"]:
            self.alive = False
            return False
        return True

    def should_reproduce(self):
        """Check if pest should reproduce today."""
        if self.age_days < self.params["reproduction_age"]:
            return False
        return np.random.random() < self.params["reproduction_rate"]


class PestSimulation:
    """Full pest simulation environment."""

    def __init__(self, initial_species="german_cockroach", initial_count=5):
        self.agents = [PestAgent(initial_species) for _ in range(initial_count)]
        self.detections = []  # List of (timestamp, x, y, species, hour)
        self.temp_c = 20.0
        self.humidity = 50
        self.season = "spring"

    def set_environment(self, temp_c, humidity, season):
        self.temp_c = temp_c
        self.humidity = humidity
        self.season = season

    def run_day(self, date):
        """Simulate one day of pest activity."""
        day_of_year = date.timetuple().tm_yday

        # Seasonal temperature
        base_temp = 15 + 10 * np.sin(2 * np.pi * (day_of_year - 80) / 365)
        self.temp_c = base_temp + np.random.normal(0, 3)

        # Season
        month = date.month
        if month in [12, 1, 2]:
            self.season = "winter"
        elif month in [3, 4, 5]:
            self.season = "spring"
        elif month in [6, 7, 8]:
            self.season = "summer"
        else:
            self.season = "fall"

        daily_detections = 0

        for hour in range(24):
            for agent in self.agents:
                if not agent.alive:
                    continue
                active = agent.step(hour, self.temp_c, self.humidity, self.season)
                if active:
                    # Detection probability (sentinel coverage)
                    if np.random.random() < 0.3:  # 30% chance of being seen
                        self.detections.append({
                            "timestamp": date.replace(hour=hour).isoformat(),
                            "x": round(agent.x, 3),
                            "y": round(agent.y, 3),
                            "species": agent.species,
                            "hour": hour,
                            "temp_c": round(self.temp_c, 1),
                        })
                        daily_detections += 1

        # Aging and reproduction
        new_agents = []
        for agent in self.agents:
            if agent.alive:
                if not agent.age_one_day():
                    continue
                if agent.should_reproduce():
                    # Add 3-6 offspring for cockroaches, 4-8 for mice
                    offspring_count = np.random.randint(3, 7)
                    for _ in range(offspring_count):
                        new_agents.append(PestAgent(agent.species,
                                                      agent.x + np.random.normal(0, 0.1),
                                                      agent.y + np.random.normal(0, 0.1)))
        self.agents.extend(new_agents)

        # Cap population (realistic carrying capacity)
        max_pop = 500
        if len(self.agents) > max_pop:
            self.agents = np.random.choice(self.agents, max_pop, replace=False).tolist()

        return daily_detections

    def get_activity_log(self):
        """Return hourly activity counts for LSTM training."""
        hourly_counts = {}
        for det in self.detections:
            hour_key = det["timestamp"][:13]  # YYYY-MM-DDTHH
            hourly_counts[hour_key] = hourly_counts.get(hour_key, 0) + 1
        return hourly_counts

    def get_population(self):
        return len([a for a in self.agents if a.alive])


def generate_simulation_dataset(n_simulations=100, days=90):
    """Generate a full training dataset from multiple simulations."""
    all_sequences = []
    all_labels = []

    species_list = ["house_mouse", "norway_rat", "german_cockroach", "argentine_ant"]

    for sim_idx in range(n_simulations):
        species = species_list[sim_idx % len(species_list)]
        initial_count = np.random.randint(2, 10)

        sim = PestSimulation(species, initial_count)
        start_date = datetime(2024, 1, 1) + timedelta(days=np.random.randint(0, 365))

        for day in range(days):
            sim.run_day(start_date + timedelta(days=day))

        # Get hourly activity
        activity = sim.get_activity_log()

        # Convert to 168-hour (7-day) sequences
        counts = list(activity.values())
        if len(counts) >= 168:
            for i in range(0, len(counts) - 168, 24):  # sliding window
                seq = counts[i:i+168]
                all_sequences.append(seq)

                # Label: pattern type
                pattern_idx = species_list.index(species)
                all_labels.append(pattern_idx)

        if (sim_idx + 1) % 10 == 0:
            print(f"  Simulation {sim_idx+1}/{n_simulations}: "
                  f"{sim.get_population()} agents, {len(sim.detections)} detections")

    return np.array(all_sequences, dtype=np.float32), np.array(all_labels)


if __name__ == "__main__":
    print("Running pest behavior simulations...")
    seqs, labels = generate_simulation_dataset(n_simulations=100, days=90)
    print(f"\nGenerated {len(seqs)} activity sequences")
    np.save("data/sim_activity_sequences.npy", seqs)
    np.save("data/sim_activity_labels.npy", labels)
    print("Saved to data/")
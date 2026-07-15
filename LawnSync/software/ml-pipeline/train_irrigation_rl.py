"""
LawnSync ML Pipeline — IrrigationRL: DQN Irrigation Scheduler

Deep Q-Network that learns optimal irrigation policies to minimize
water consumption while maintaining soil moisture in the optimal band.

State (12-dim): soil moisture, 24h forecast (temp, hum, rain prob, rain amt),
                wind, solar irradiance, days since last irrigation,
                grass type (encoded), soil type (encoded)
Action: (zone_index, duration_minutes) — discrete
Reward: +10 if moisture in optimal band, -5 if below wilting point,
        -2 per liter used, -10 if runoff detected

Trained on 500K simulated episodes + online fine-tuning.
"""

import os
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from collections import deque
import random

# ---------------------------------------------------------------------------
# Environment: Soil Moisture Simulation (simplified Hydrus-1D)
# ---------------------------------------------------------------------------

class SoilEnv:
    """Simplified soil moisture dynamics environment.

    Simulates:
    - Evapotranspiration (Penman-Monteith simplified)
    - Rainfall infiltration
    - Irrigation effect
    - Drainage (gravity)
    """

    def __init__(self, n_zones: int = 4):
        self.n_zones = n_zones
        self.field_capacity = 30.0   # % VWC
        self.wilting_point = 12.0     # % VWC
        self.optimal_low = 18.0       # % VWC
        self.optimal_high = 28.0      # % VWC
        self.max_duration = 30        # minutes
        self.reset()

    def reset(self):
        self.moisture = np.random.uniform(15, 25, self.n_zones)
        self.days_since_irrigation = np.zeros(self.n_zones, dtype=int)
        self.day = 0
        self.max_days = 30  # One month episode
        return self._get_state()

    def _get_state(self):
        """Return 12-dimensional state vector."""
        # Simplified: use zone 1 as representative (in production, per-zone)
        state = np.zeros(12, dtype=np.float32)
        state[0] = np.mean(self.moisture) / self.field_capacity
        # 24h forecast features (simplified)
        state[1] = 22.0 / 40.0   # forecast temp (normalized)
        state[2] = 0.60           # forecast humidity
        state[3] = 0.20           # rain probability
        state[4] = 5.0 / 50.0    # rain amount (mm, normalized)
        state[5] = 3.5 / 20.0    # wind speed (normalized)
        state[6] = 650.0 / 1000.0 # solar irradiance (normalized)
        state[7] = np.mean(self.days_since_irrigation) / 7.0
        state[8] = 1.0            # grass type: Kentucky Bluegrass (encoded)
        state[9] = 0.5            # soil type: loam (encoded)
        state[10] = np.min(self.moisture) / self.field_capacity
        state[11] = np.max(self.moisture) / self.field_capacity
        return state

    def step(self, action: tuple):
        """Action: (zone_index, duration_minutes)"""
        zone, duration = action

        # Apply irrigation
        water_applied = duration * 2.0  # 2 L/min
        if zone >= 0 and zone < self.n_zones:
            # Moisture increase (simplified: 1 L → 0.5% VWC per zone area)
            self.moisture[zone] += duration * 0.5
            self.days_since_irrigation[zone] = 0

        # Daily dynamics
        for z in range(self.n_zones):
            # Evapotranspiration (simplified)
            et = 2.0 + np.random.normal(0, 0.5)  # mm/day
            self.moisture[z] -= et * 0.4  # Convert ET to VWC change

            # Rainfall
            rain = np.random.exponential(3.0) if random.random() < 0.2 else 0
            self.moisture[z] += rain * 0.3

            # Drainage (if above field capacity)
            if self.moisture[z] > self.field_capacity:
                drain = (self.moisture[z] - self.field_capacity) * 0.5
                self.moisture[z] -= drain
            else:
                drain = 0

            self.moisture[z] = np.clip(self.moisture[z], 0, 50)
            self.days_since_irrigation[z] += 1

        # Calculate reward
        reward = 0
        for z in range(self.n_zones):
            if self.optimal_low <= self.moisture[z] <= self.optimal_high:
                reward += 10 / self.n_zones
            elif self.moisture[z] < self.wilting_point:
                reward -= 5 / self.n_zones
            if self.moisture[z] > self.field_capacity:
                reward -= 10 / self.n_zones  # runoff penalty

        reward -= (water_applied / 60.0) * 2  # water cost

        self.day += 1
        done = self.day >= self.max_days
        return self._get_state(), reward, done, {}


# ---------------------------------------------------------------------------
# DQN Model
# ---------------------------------------------------------------------------

class DQNet(nn.Module):
    """Deep Q-Network for irrigation scheduling."""
    def __init__(self, state_dim: int = 12, n_zones: int = 4,
                 max_duration: int = 30):
        super().__init__()
        self.n_zones = n_zones
        self.max_duration = max_duration
        self.n_actions = n_zones * max_duration + 1  # +1 for "skip"

        self.net = nn.Sequential(
            nn.Linear(state_dim, 128),
            nn.ReLU(),
            nn.Linear(128, 128),
            nn.ReLU(),
            nn.Linear(128, self.n_actions),
        )

    def forward(self, x):
        return self.net(x)

    def action_to_tuple(self, action_idx: int):
        """Convert action index to (zone, duration_minutes)."""
        if action_idx == self.n_actions - 1:
            return (-1, 0)  # Skip irrigation
        zone = action_idx // self.max_duration
        duration = (action_idx % self.max_duration) + 1
        return (zone, duration)


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train_dqn(episodes: int = 50000):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[IrrigationRL] Training DQN on {device}")

    env = SoilEnv(n_zones=4)
    model = DQNet().to(device)
    target_net = DQNet().to(device)
    target_net.load_state_dict(model.state_dict())

    optimizer = optim.Adam(model.parameters(), lr=1e-4)
    criterion = nn.SmoothL1Loss()

    replay_buffer = deque(maxlen=100000)
    batch_size = 64
    gamma = 0.95
    eps_start = 1.0
    eps_end = 0.05
    eps_decay = 50000
    target_update = 1000

    MODEL_DIR = "./models"
    os.makedirs(MODEL_DIR, exist_ok=True)

    total_rewards = []
    total_water = []

    for episode in range(episodes):
        state = env.reset()
        ep_reward = 0
        ep_water = 0
        done = False

        epsilon = eps_end + (eps_start - eps_end) * np.exp(-episode / eps_decay)

        while not done:
            # Epsilon-greedy action
            if random.random() < epsilon:
                action_idx = random.randint(0, model.n_actions - 1)
            else:
                with torch.no_grad():
                    q_vals = model(torch.FloatTensor(state).unsqueeze(0).to(device))
                    action_idx = q_vals.argmax(1).item()

            zone, duration = model.action_to_tuple(action_idx)
            next_state, reward, done, _ = env.step((zone, duration))
            ep_reward += reward
            if zone >= 0:
                ep_water += duration * 2.0  # liters

            replay_buffer.append((state, action_idx, reward, next_state, done))
            state = next_state

            # Train on batch
            if len(replay_buffer) >= batch_size:
                batch = random.sample(replay_buffer, batch_size)
                states, actions, rewards, next_states, dones = zip(*batch)

                states = torch.FloatTensor(states).to(device)
                actions = torch.LongTensor(actions).to(device)
                rewards = torch.FloatTensor(rewards).to(device)
                next_states = torch.FloatTensor(next_states).to(device)
                dones = torch.FloatTensor(dones).to(device)

                q_values = model(states).gather(1, actions.unsqueeze(1)).squeeze()
                with torch.no_grad():
                    next_q = target_net(next_states).max(1)[0]
                    target = rewards + gamma * next_q * (1 - dones)

                loss = criterion(q_values, target)
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

        total_rewards.append(ep_reward)
        total_water.append(ep_water)

        if (episode + 1) % target_update == 0:
            target_net.load_state_dict(model.state_dict())

        if (episode + 1) % 1000 == 0:
            avg_r = np.mean(total_rewards[-1000:])
            avg_w = np.mean(total_water[-1000:])
            print(f"Episode {episode+1}/{episodes} — "
                  f"Avg Reward: {avg_r:.1f}, Avg Water: {avg_w:.0f}L, Eps: {epsilon:.3f}")

    # Save model
    torch.save(model.state_dict(), os.path.join(MODEL_DIR, "irrigation_dqn.pth"))
    print(f"\n[IrrigationRL] Training complete. Model saved.")

    # Export to ONNX
    model.eval()
    dummy = torch.randn(1, 12)
    torch.onnx.export(model, dummy, os.path.join(MODEL_DIR, "irrigation_dqn.onnx"),
                      input_names=["state"], output_names=["q_values"],
                      opset_version=13)

    return model


if __name__ == "__main__":
    train_dqn()
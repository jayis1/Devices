"""
MenoSync — CoolingOptimizer DQN Training Script

Deep Q-Network (DQN) for optimizing pre-emptive cooling strategy
to minimize hot flash severity and frequency.

The agent learns the optimal cooling strategy by observing:
- Current physiological state (skin temp, EDA, HR, HRV)
- Hot flash prediction (probability, time to onset)
- Room conditions (ambient temp, radiant temp, humidity)
- Time of day, day of cycle

Actions:
- HVAC mode: off / cool / fan (0, 1, 3)
- Target temperature: 20-26°C in 1°C steps
- Shade position: 0-100% in 20% steps

Reward: -hot_flash_severity (if occurs) + energy_cost_penalty
        + comfort_maintained_bonus

Usage:
  python train_cooling.py --episodes 10000
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from collections import deque
import random


class CoolingEnv:
    """Simplified simulation of menopause cooling environment.

    Models:
    - Skin temperature dynamics (rises before hot flash, falls with cooling)
    - Hot flash trigger (temp + EDA + ambient threshold)
    - Cooling effect (HVAC reduces ambient, shades reduce radiant)
    - Energy cost
    """
    def __init__(self):
        self.skin_temp = 33.0  # °C
        self.eda = 5.0  # µS
        self.ambient_temp = 24.0  # °C
        self.radiant_temp = 24.0  # °C
        self.humidity = 45.0  # %
        self.hvac_mode = 0  # 0=off, 1=cool, 3=fan
        self.target_temp = 24.0  # °C
        self.shade_pct = 0  # 0-100
        self.step_count = 0
        self.max_steps = 120  # 2 hours at 1-min steps

    def reset(self):
        self.skin_temp = 33.0 + random.uniform(-0.5, 0.5)
        self.eda = 5.0 + random.uniform(-1, 3)
        self.ambient_temp = 22.0 + random.uniform(-2, 6)
        self.radiant_temp = self.ambient_temp + random.uniform(0, 3)
        self.hvac_mode = 0
        self.target_temp = 24.0
        self.shade_pct = 0
        self.step_count = 0
        return self._get_state()

    def _get_state(self):
        return np.array([
            self.skin_temp - 33.0,
            self.eda - 5.0,
            self.ambient_temp - 23.0,
            self.radiant_temp - self.ambient_temp,
            self.humidity - 45.0,
            float(self.hvac_mode) / 3.0,
            self.target_temp - 23.0,
            self.shade_pct / 100.0,
            np.sin(self.step_count * 2 * np.pi / 1440),  # time of day
        ], dtype=np.float32)

    def step(self, action):
        # action: (hvac_mode_idx, target_temp_idx, shade_idx)
        hvac_modes = [0, 1, 3]  # off, cool, fan
        self.hvac_mode = hvac_modes[action[0]]
        self.target_temp = 20.0 + action[1]  # 20-26°C
        self.shade_pct = action[2] * 20  # 0-100 in 20% steps

        # Apply cooling effect
        if self.hvac_mode == 1:  # cool
            cooling_rate = 0.15
            self.ambient_temp += (self.target_temp - self.ambient_temp) * cooling_rate
        elif self.hvac_mode == 3:  # fan
            self.ambient_temp -= 0.05  # slight cooling from air movement

        # Shade effect on radiant temp
        self.radiant_temp = self.ambient_temp + 3.0 * (1.0 - self.shade_pct / 100.0)

        # Skin temp dynamics: rises from ambient + EDA, falls with cooling
        temp_stress = max(0, self.ambient_temp - 26.0) + max(0, self.radiant_temp - 27.0)
        eda_stress = max(0, self.eda - 10.0) * 0.1
        self.skin_temp += 0.05 * temp_stress + 0.02 * eda_stress - 0.03
        self.skin_temp = max(31.0, min(38.0, self.skin_temp))

        # EDA dynamics
        self.eda += 0.5 * temp_stress + random.uniform(-0.5, 0.5)
        self.eda = max(2.0, min(40.0, self.eda))

        # Check if hot flash occurs
        hot_flash = (self.skin_temp > 36.5 and self.eda > 15) or \
                    (self.skin_temp > 37.0 and self.ambient_temp > 26.0)

        # Reward
        severity = 0.0
        if hot_flash:
            severity = (self.skin_temp - 36.5) * 2.0 + (self.eda - 15) * 0.1
            severity = min(3.0, severity)

        energy_cost = 0.0
        if self.hvac_mode == 1:
            energy_cost = 0.5  # cooling is energy-intensive
        elif self.hvac_mode == 3:
            energy_cost = 0.1  # fan is cheap

        comfort_bonus = 0.0
        if 20.0 <= self.ambient_temp <= 24.0:
            comfort_bonus = 0.2

        reward = -severity * 2.0 - energy_cost + comfort_bonus

        self.step_count += 1
        done = self.step_count >= self.max_steps or hot_flash
        return self._get_state(), reward, done, {"hot_flash": hot_flash, "severity": severity}


class DQN(nn.Module):
    """Deep Q-Network for cooling optimization.

    Input:  9-dimensional state
    Output: Q-values for each action combination
            (3 hvac × 7 temps × 6 shades = 126 actions)
    """
    def __init__(self, state_size=9, action_size=126, hidden_size=256):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size // 2),
            nn.ReLU(),
            nn.Linear(hidden_size // 2, action_size),
        )

    def forward(self, x):
        return self.net(x)


def action_to_indices(action_idx):
    """Convert flat action index to (hvac, temp, shade) indices."""
    shade_idx = action_idx % 6
    temp_idx = (action_idx // 6) % 7
    hvac_idx = action_idx // 42
    return (hvac_idx, temp_idx, shade_idx)


def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training CoolingOptimizer DQN on {device}")

    env = CoolingEnv()
    state_size = 9
    action_size = 126  # 3 × 7 × 6

    policy_net = DQN(state_size, action_size).to(device)
    target_net = DQN(state_size, action_size).to(device)
    target_net.load_state_dict(policy_net.state_dict())
    target_net.eval()

    optimizer = optim.Adam(policy_net.parameters(), lr=1e-4)
    criterion = nn.SmoothL1Loss()

    memory = deque(maxlen=10000)
    batch_size = 64
    gamma = 0.95
    eps_start = 1.0
    eps_end = 0.05
    eps_decay = 0.9995
    target_update = 100
    steps = 0

    best_avg_reward = -float("inf")
    eps = eps_start

    for episode in range(args.episodes):
        state = env.reset()
        total_reward = 0
        hot_flashes = 0

        for t in range(env.max_steps):
            eps = max(eps_end, eps_start * (eps_decay ** steps))

            if random.random() < eps:
                action_idx = random.randint(0, action_size - 1)
            else:
                with torch.no_grad():
                    s = torch.tensor(state, device=device).unsqueeze(0)
                    q_vals = policy_net(s)
                    action_idx = q_vals.argmax(1).item()

            action = action_to_indices(action_idx)
            next_state, reward, done, info = env.step(action)
            total_reward += reward
            if info["hot_flash"]:
                hot_flashes += 1

            memory.append((state, action_idx, reward, next_state, done))
            state = next_state
            steps += 1

            if len(memory) >= batch_size:
                batch = random.sample(memory, batch_size)
                states, actions, rewards, next_states, dones = zip(*batch)

                states = torch.tensor(states, device=device)
                actions = torch.tensor(actions, device=device, dtype=torch.long)
                rewards = torch.tensor(rewards, device=device)
                next_states = torch.tensor(next_states, device=device)
                dones = torch.tensor(dones, device=device, dtype=torch.float32)

                q_values = policy_net(states).gather(1, actions.unsqueeze(1)).squeeze()
                with torch.no_grad():
                    next_q = target_net(next_states).max(1)[0]
                    target = rewards + gamma * next_q * (1 - dones)

                loss = criterion(q_values, target)
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

            if steps % target_update == 0:
                target_net.load_state_dict(policy_net.state_dict())

            if done:
                break

        if (episode + 1) % 100 == 0:
            avg_reward = total_reward / env.max_steps
            print(f"Episode {episode+1}/{args.episodes}: "
                  f"reward={total_reward:.1f} hot_flashes={hot_flashes} "
                  f"eps={eps:.3f}")
            if avg_reward > best_avg_reward:
                best_avg_reward = avg_reward
                torch.save(policy_net.state_dict(), f"{args.output}/cooling_dqn_best.pth")
                print(f"  → New best avg reward: {avg_reward:.3f}")

    print(f"\nDone. Best avg reward: {best_avg_reward:.3f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--episodes", type=int, default=10000)
    parser.add_argument("--output", type=str, default="models")
    args = parser.parse_args()
    import os
    os.makedirs(args.output, exist_ok=True)
    train(args)
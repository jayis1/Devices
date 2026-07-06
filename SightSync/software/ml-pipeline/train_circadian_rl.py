"""
SightSync ML Pipeline — Circadian Lamp Policy (DQN)
=====================================================

Trains a Deep Q-Network to learn the optimal circadian
lamp color temperature and brightness policy.

State: [hour_of_day, ambient_lux, fatigue_score, user_preference]
Actions: [CCT (1800-6500 K), brightness (0-100%)]
Reward: -fatigue_score + user_override_penalty + circadian_alignment_bonus

License: MIT
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import random
from collections import deque
import os

MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "firmware", "hub", "models")

# ── DQN Model ────────────────────────────────────────────────────────

class LampDQN(nn.Module):
    def __init__(self, state_size=4, action_size=48):
        """Actions: 6 CCT levels × 8 brightness levels = 48 discrete actions."""
        super().__init__()
        self.fc1 = nn.Linear(state_size, 64)
        self.fc2 = nn.Linear(64, 64)
        self.fc3 = nn.Linear(64, action_size)

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.relu(self.fc2(x))
        return self.fc3(x)


# ── Action Space ─────────────────────────────────────────────────────

CCT_LEVELS = [1800, 2700, 3500, 4500, 5500, 6500]
BRIGHTNESS_LEVELS = [15, 30, 45, 60, 70, 80, 90, 100]
ACTIONS = [(cct, bright) for cct in CCT_LEVELS for bright in BRIGHTNESS_LEVELS]
N_ACTIONS = len(ACTIONS)


# ── Replay Buffer ────────────────────────────────────────────────────

class ReplayBuffer:
    def __init__(self, capacity=10000):
        self.buffer = deque(maxlen=capacity)

    def push(self, state, action, reward, next_state, done):
        self.buffer.append((state, action, reward, next_state, done))

    def sample(self, batch_size):
        batch = random.sample(self.buffer, min(batch_size, len(self.buffer)))
        states, actions, rewards, next_states, dones = zip(*batch)
        return (np.array(states), np.array(actions), np.array(rewards),
                np.array(next_states), np.array(dones))

    def __len__(self):
        return len(self.buffer)


# ── Reward Function ──────────────────────────────────────────────────

def compute_reward(state, action_idx, fatigue_score):
    """Compute reward for a lamp action given the current state."""
    hour, ambient_lux, fatigue, preference = state
    cct, brightness = ACTIONS[action_idx]

    # Circadian alignment: warm at night, cool during day
    if 6 <= hour < 10:       # morning: warm
        circadian_bonus = 1.0 - abs(cct - 3000) / 5000
    elif 10 <= hour < 17:    # day: cool
        circadian_bonus = 1.0 - abs(cct - 5500) / 5000
    elif 17 <= hour < 22:    # evening: warm
        circadian_bonus = 1.0 - abs(cct - 3500) / 5000
    else:                    # night: very warm
        circadian_bonus = 1.0 - abs(cct - 1800) / 5000

    # Fatigue reduction: higher brightness if fatigued
    if fatigue > 60:
        fatigue_bonus = brightness / 100.0 * 0.5
    else:
        fatigue_bonus = 0.0

    # Ambient light compensation
    if ambient_lux < 300 and hour >= 6 and hour < 22:
        ambient_bonus = brightness / 100.0 * 0.3
    else:
        ambient_bonus = 0.0

    reward = circadian_bonus + fatigue_bonus + ambient_bonus - fatigue_score / 200.0
    return reward


# ── Training ─────────────────────────────────────────────────────────

def train(episodes=5000):
    print("=== SightSync Circadian Lamp DQN Training ===")

    state_size = 4
    action_size = N_ACTIONS

    policy_net = LampDQN(state_size, action_size)
    target_net = LampDQN(state_size, action_size)
    target_net.load_state_dict(policy_net.state_dict())

    optimizer = optim.Adam(policy_net.parameters(), lr=0.001)
    buffer = ReplayBuffer(10000)

    gamma = 0.95
    epsilon = 1.0
    epsilon_min = 0.01
    epsilon_decay = 0.995
    target_update = 100

    for episode in range(episodes):
        # Random initial state
        hour = random.uniform(0, 24)
        ambient_lux = random.uniform(100, 1000)
        fatigue = random.uniform(0, 100)
        preference = random.uniform(0, 1)
        state = np.array([hour, ambient_lux, fatigue, preference])

        total_reward = 0
        for step in range(24):  # 24 hours
            # Epsilon-greedy action
            if random.random() < epsilon:
                action = random.randint(0, action_size - 1)
            else:
                with torch.no_grad():
                    q_values = policy_net(torch.tensor(state, dtype=torch.float32))
                    action = q_values.argmax().item()

            # Compute reward
            reward = compute_reward(state, action, fatigue)
            total_reward += reward

            # Transition: advance hour, update fatigue
            next_hour = (hour + 1) % 24
            next_fatigue = max(0, fatigue - 5 + random.uniform(-3, 3))
            next_state = np.array([next_hour, ambient_lux, next_fatigue, preference])
            done = (step == 23)

            buffer.push(state, action, reward, next_state, done)
            state = next_state
            hour = next_hour
            fatigue = next_fatigue

            # Train on batch
            if len(buffer) > 32:
                s, a, r, s2, d = buffer.sample(32)
                s = torch.tensor(s, dtype=torch.float32)
                a = torch.tensor(a, dtype=torch.long)
                r = torch.tensor(r, dtype=torch.float32)
                s2 = torch.tensor(s2, dtype=torch.float32)
                d = torch.tensor(d, dtype=torch.float32)

                q_values = policy_net(s).gather(1, a.unsqueeze(1)).squeeze()
                next_q = target_net(s2).max(1)[0].detach()
                target = r + gamma * next_q * (1 - d)

                loss = nn.MSELoss()(q_values, target)
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

            if episode % target_update == 0:
                target_net.load_state_dict(policy_net.state_dict())

        epsilon = max(epsilon_min, epsilon * epsilon_decay)

        if (episode + 1) % 500 == 0:
            print(f"Episode {episode+1}: avg_reward={total_reward/24:.3f} epsilon={epsilon:.3f}")

    # Save model
    os.makedirs(MODEL_DIR, exist_ok=True)
    model_path = os.path.join(MODEL_DIR, "lamp_dqn.pt")
    torch.save(policy_net.state_dict(), model_path)
    print(f"Model saved: {model_path}")

    return policy_net


if __name__ == "__main__":
    train()
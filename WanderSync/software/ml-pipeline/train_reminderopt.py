#!/usr/bin/env python3
"""
WanderSync — ReminderOpt Training Script

Reinforcement learning (DQN) for personalized reminder timing optimization.
Learns when the person is most receptive to reminders based on activity
state, time-of-day, and historical acknowledgment patterns.

7-model ML pipeline: model 6 of 7.

Output: DQN model (cloud deployment, personalized per patient).
"""
from __future__ import annotations

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from collections import deque
import random


class ReminderDQN(nn.Module):
    """Deep Q-Network for reminder timing optimization.

    State (12): activity_class, time_of_day, day_of_week, last_reminder_h_ago,
                last_reminder_acked, time_since_meal_h, time_since_med_h,
                activity_level, room, heart_rate, historical_adherence, anomaly_score
    Actions (5): no_reminder, gentle_tone, family_voice, repeat_reminder, escalate
    """

    def __init__(self, state_dim: int = 12, n_actions: int = 5) -> None:
        super().__init__()
        self.fc1 = nn.Linear(state_dim, 128)
        self.fc2 = nn.Linear(128, 64)
        self.fc3 = nn.Linear(64, n_actions)

    def forward(self, state):
        x = torch.relu(self.fc1(state))
        x = torch.relu(self.fc2(x))
        return self.fc3(x)


class ReminderEnvironment:
    """Simulated environment for reminder RL training.

    Production: use 200,000 reminder events from 500 patients over 6 months.
    """

    def __init__(self) -> None:
        self.state_dim = 12
        self.n_actions = 5
        self.reset()

    def reset(self):
        self.state = np.random.rand(self.state_dim).astype(np.float32)
        self.state[1] = np.random.randint(0, 24)  # time of day
        self.state[3] = 0  # last reminder hours ago
        self.state[5] = np.random.uniform(0, 4)  # time since meal
        self.state[6] = np.random.uniform(0, 8)  # time since med
        self.step_count = 0
        return self.state

    def step(self, action: int):
        """Returns (next_state, reward, done)"""
        reward = 0.0

        if action == 0:  # no reminder
            reward = 0.0
        elif action == 1:  # gentle tone
            if self.state[1] > 7 and self.state[1] < 21:  # daytime
                reward = 5.0  # acknowledged
            else:
                reward = -1.0  # nighttime, ignored
        elif action == 2:  # family voice
            ack_prob = 0.7 + 0.2 * (1 - self.state[10])  # higher with low historical adherence
            if np.random.random() < ack_prob:
                reward = 10.0
            else:
                reward = -2.0
        elif action == 3:  # repeat
            if self.state[4] < 0.5:  # last wasn't acked
                reward = 3.0
            else:
                reward = -3.0  # reminder fatigue
        elif action == 4:  # escalate
            reward = -5.0  # last resort

        # Update state
        self.state[1] = (self.state[1] + 1) % 24  # advance time
        self.state[3] += 1  # hours since last reminder
        self.state[10] = 0.7 * self.state[10] + 0.3 * (1.0 if reward > 0 else 0.0)

        self.step_count += 1
        done = self.step_count >= 24  # One day

        return self.state.copy(), reward, done


def train_dqn(episodes: int = 5000, gamma: float = 0.95, epsilon_start: float = 1.0,
              epsilon_end: float = 0.05, lr: float = 1e-3):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training ReminderOpt DQN on {device}")

    env = ReminderEnvironment()
    model = ReminderDQN().to(device)
    target = ReminderDQN().to(device)
    target.load_state_dict(model.state_dict())

    optimizer = optim.Adam(model.parameters(), lr=lr)
    buffer = deque(maxlen=10000)
    batch_size = 64
    target_update = 100

    epsilon = epsilon_start
    steps = 0

    for episode in range(episodes):
        state = env.reset()
        total_reward = 0.0

        while True:
            # Epsilon-greedy action
            if random.random() < epsilon:
                action = random.randint(0, env.n_actions - 1)
            else:
                with torch.no_grad():
                    q = model(torch.from_numpy(state).float().to(device))
                    action = q.argmax().item()

            next_state, reward, done = env.step(action)
            buffer.append((state, action, reward, next_state, done))
            state = next_state
            total_reward += reward
            steps += 1

            # Train
            if len(buffer) >= batch_size and steps % 4 == 0:
                batch = random.sample(buffer, batch_size)
                states, actions, rewards, next_states, dones = zip(*batch)

                states = torch.tensor(np.array(states), dtype=torch.float32).to(device)
                actions = torch.tensor(actions, dtype=torch.long).to(device)
                rewards = torch.tensor(rewards, dtype=torch.float32).to(device)
                next_states = torch.tensor(np.array(next_states), dtype=torch.float32).to(device)
                dones = torch.tensor(dones, dtype=torch.float32).to(device)

                q_values = model(states).gather(1, actions.unsqueeze(1)).squeeze()
                with torch.no_grad():
                    next_q = target(next_states).max(1)[0]
                    target_q = rewards + gamma * next_q * (1 - dones)

                loss = nn.functional.mse_loss(q_values, target_q)
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

            if steps % target_update == 0:
                target.load_state_dict(model.state_dict())

            if done:
                break

        epsilon = max(epsilon_end, epsilon * 0.995)

        if (episode + 1) % 1000 == 0:
            print(f"Episode {episode+1}/{episodes}: avg_reward={total_reward:.1f} "
                  f"epsilon={epsilon:.3f}")

    return model


if __name__ == "__main__":
    model = train_dqn(episodes=5000)
    torch.save(model.state_dict(), "reminderopt.pt")
    print("ReminderOpt DQN saved to reminderopt.pt")
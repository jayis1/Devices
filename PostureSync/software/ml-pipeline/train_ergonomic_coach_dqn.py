"""
ErgonomicCoach DQN Training Script
Optimal posture correction timing to maximize adherence without notification fatigue

State: (current_score, time_since_last, response_rate, time_of_day, activity)
Action: (0=wait, 1=remind_now, 2=remind_5min, 3=remind_10min)
Reward: posture_improvement × response_rate - notification_penalty
"""

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import random
from collections import deque
import json
import os

# State space
STATE_DIM = 5  # score, time_since_last, response_rate, time_of_day, activity
NUM_ACTIONS = 4  # wait, remind_now, remind_5min, remind_10min


class QNetwork(nn.Module):
    """Q-Network for DQN"""

    def __init__(self, state_dim=STATE_DIM, num_actions=NUM_ACTIONS, hidden=64):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, num_actions),
        )

    def forward(self, x):
        return self.net(x)


class ReplayBuffer:
    """Experience replay buffer"""

    def __init__(self, capacity=10000):
        self.buffer = deque(maxlen=capacity)

    def push(self, state, action, reward, next_state, done):
        self.buffer.append((state, action, reward, next_state, done))

    def sample(self, batch_size):
        batch = random.sample(self.buffer, batch_size)
        states, actions, rewards, next_states, dones = zip(*batch)
        return (
            torch.FloatTensor(states),
            torch.LongTensor(actions),
            torch.FloatTensor(rewards),
            torch.FloatTensor(next_states),
            torch.FloatTensor(dones),
        )

    def __len__(self):
        return len(self.buffer)


class PostureEnv:
    """Simplified posture correction environment"""

    def __init__(self):
        self.score = 80
        self.time_since_last = 0
        self.response_rate = 0.6
        self.time_of_day = 9.0  # 9 AM
        self.activity = 0  # 0=desk, 1=walking, 2=standing
        self.step_count = 0

    def reset(self):
        self.score = np.random.uniform(60, 90)
        self.time_since_last = np.random.randint(0, 300)
        self.response_rate = np.random.uniform(0.3, 0.9)
        self.time_of_day = np.random.uniform(8, 18)
        self.activity = np.random.randint(0, 3)
        self.step_count = 0
        return self._get_state()

    def _get_state(self):
        return np.array([
            self.score / 100.0,
            min(self.time_since_last / 600.0, 1.0),
            self.response_rate,
            self.time_of_day / 24.0,
            self.activity / 2.0,
        ])

    def step(self, action):
        self.step_count += 1

        # Apply action
        if action == 1:  # remind now
            if self.score < 60:
                improvement = np.random.uniform(10, 30) * self.response_rate
                self.score = min(100, self.score + improvement)
                reward = improvement * self.response_rate - 0.5  # notification cost
            else:
                reward = -0.5  # unnecessary reminder
            self.time_since_last = 0
        elif action == 2:  # remind in 5 min
            self.time_since_last += 300
            if self.score < 60:
                reward = 5 * self.response_rate
            else:
                reward = 0
        elif action == 3:  # remind in 10 min
            self.time_since_last += 600
            if self.score < 50:
                reward = 3 * self.response_rate
            else:
                reward = 0
        else:  # wait
            self.time_since_last += 60
            # Posture naturally degrades
            self.score = max(30, self.score - np.random.uniform(0.5, 2.0))
            reward = self.score / 100.0 * 0.1

        # Simulate posture drift
        self.score = max(30, min(100, self.score + np.random.normal(0, 1)))
        self.time_of_day += 0.01

        done = self.step_count >= 500 or self.time_of_day > 18

        return self._get_state(), reward, done, {}


def train_dqn(episodes: int = 2000, save_dir: str = "models"):
    """Train ErgonomicCoach DQN"""

    env = PostureEnv()
    q_net = QNetwork()
    target_net = QNetwork()
    target_net.load_state_dict(q_net.state_dict())

    optimizer = optim.Adam(q_net.parameters(), lr=0.001)
    buffer = ReplayBuffer(capacity=10000)

    gamma = 0.95
    epsilon = 1.0
    epsilon_min = 0.01
    epsilon_decay = 0.995
    batch_size = 64
    target_update = 10

    rewards_history = []

    for episode in range(episodes):
        state = env.reset()
        total_reward = 0

        for step in range(500):
            # Epsilon-greedy action
            if random.random() < epsilon:
                action = random.randint(0, NUM_ACTIONS - 1)
            else:
                with torch.no_grad():
                    q_values = q_net(torch.FloatTensor(state))
                    action = q_values.argmax().item()

            next_state, reward, done, _ = env.step(action)
            buffer.push(state, action, reward, next_state, float(done))
            state = next_state
            total_reward += reward

            if done:
                break

            # Train
            if len(buffer) >= batch_size:
                s, a, r, ns, d = buffer.sample(batch_size)
                q_values = q_net(s).gather(1, a.unsqueeze(1)).squeeze()
                with torch.no_grad():
                    max_q = target_net(ns).max(1)[0]
                    target = r + gamma * max_q * (1 - d)
                loss = nn.MSELoss()(q_values, target)
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

        # Update target network
        if episode % target_update == 0:
            target_net.load_state_dict(q_net.state_dict())

        # Decay epsilon
        epsilon = max(epsilon_min, epsilon * epsilon_decay)

        rewards_history.append(total_reward)

        if (episode + 1) % 100 == 0:
            avg_reward = np.mean(rewards_history[-100:])
            print(f"Episode {episode+1}/{episodes} — avg_reward: {avg_reward:.2f}, epsilon: {epsilon:.3f}")

    # Save model
    os.makedirs(save_dir, exist_ok=True)
    torch.save(q_net.state_dict(), os.path.join(save_dir, "ergonomic_coach_dqn.pt"))

    # Export to ONNX
    q_net.eval()
    dummy_input = torch.randn(1, STATE_DIM)
    torch.onnx.export(
        q_net, dummy_input,
        os.path.join(save_dir, "ergonomic_coach_dqn.onnx"),
        input_names=["state"],
        output_names=["q_values"],
        dynamic_axes={"state": {0: "batch"}, "q_values": {0: "batch"}},
    )

    with open(os.path.join(save_dir, "dqn_rewards.json"), "w") as f:
        json.dump(rewards_history, f)

    print(f"DQN training complete. Model saved to {save_dir}")
    return q_net


if __name__ == "__main__":
    train_dqn(episodes=2000)
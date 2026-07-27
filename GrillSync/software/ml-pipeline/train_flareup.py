"""
GrillSync — FlareUpNet Training Script
LSTM for grill flare-up prediction 8–15 seconds in advance.

Input:  6-channel time series (5s × 10Hz = 50 timesteps)
        - Thermal max temp (°C)
        - Thermal gradient (°C/s)
        - Hot zone count (>260°C)
        - Acoustic RMS (fat-drip pattern)
        - Flame intensity (0–255)
        - Gas concentration (ppm)
Output: 2 values [flare_up_risk (0–100%), time_to_flare (×100ms)]
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW
from tqdm import tqdm


# === Model ===
class FlareUpNet(nn.Module):
    def __init__(self, input_size=6, hidden_size=64, output_size=2, seq_len=50):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.fc2 = nn.Linear(32, 16)
        self.fc3 = nn.Linear(16, output_size)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x: (batch, seq_len, input_size)
        out, (hn, cn) = self.lstm(x)
        out = out[:, -1, :]  # Take last timestep
        out = self.relu(self.fc1(out))
        out = self.relu(self.fc2(out))
        out = self.fc3(out)
        # Risk: 0–1 (sigmoid), ETA: 0–150 (scaled)
        risk = self.sigmoid(out[:, 0]) * 100
        eta = self.sigmoid(out[:, 1]) * 15000  # max 15s in ms
        return torch.stack([risk, eta], dim=1)


# === Dataset ===
class FlareUpDataset(Dataset):
    def __init__(self, sessions, seq_len=50, lookahead=150):
        """
        seq_len: 50 timesteps (5s at 10Hz)
        lookahead: 150 timesteps ahead (15s at 10Hz)
        """
        self.samples = []
        for session in sessions:
            thermal_max = np.array(session["thermal_history"]["max_temp"])
            thermal_grad = np.array(session["thermal_history"]["gradient"])
            hot_zones = np.array(session["thermal_history"]["hot_zones"])
            acoustic = np.array(session["thermal_history"]["acoustic"])
            flame = np.array(session["thermal_history"]["flame"])
            gas = np.array(session["thermal_history"]["gas"])

            flare_events = session.get("flare_up_events", [])
            flare_times = set()
            for evt in flare_events:
                # Mark timesteps within lookahead window before flare
                start_ts = int(evt["timestamp_s"] * 10) - lookahead
                end_ts = int(evt["timestamp_s"] * 10)
                for t in range(max(0, start_ts), end_ts):
                    flare_times.add(t)

            for i in range(0, len(thermal_max) - seq_len - lookahead, 5):
                window = np.stack([
                    thermal_max[i:i + seq_len],
                    thermal_grad[i:i + seq_len],
                    hot_zones[i:i + seq_len],
                    acoustic[i:i + seq_len],
                    flame[i:i + seq_len],
                    gas[i:i + seq_len],
                ], axis=1).astype(np.float32)

                # Normalize
                window[0] /= 400.0  # Temperature
                window[1] /= 50.0   # Gradient
                window[2] /= 20.0   # Hot zones
                window[3] /= 1000.0  # Acoustic
                window[4] /= 255.0  # Flame
                window[5] /= 21000.0  # Gas

                has_flare = 1 if (i + seq_len) in flare_times else 0
                self.samples.append((window, has_flare))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        window, label = self.samples[idx]
        return torch.from_numpy(window), torch.tensor([label, label], dtype=torch.float32)


# === Training ===
def train_model(data_path, output_path, epochs=50, batch_size=128, lr=1e-3):
    """Train FlareUpNet on grill session data with flare-up events."""
    print("Generating synthetic flare-up training data...")
    sessions = generate_synthetic_data(n_sessions=500)

    dataset = FlareUpDataset(sessions)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=4)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = FlareUpNet().to(device)
    optimizer = AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    # Weighted loss for class imbalance (flare-ups are rare)
    pos_weight = torch.tensor([10.0, 10.0]).to(device)
    criterion = nn.BCEWithLogitsLoss(pos_weight=pos_weight)

    print(f"Training FlareUpNet on {len(dataset)} samples, {epochs} epochs")
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        for batch_x, batch_y in tqdm(loader, desc=f"Epoch {epoch+1}/{epochs}"):
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()

        print(f"Epoch {epoch+1}: loss={total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), output_path)
    print(f"Model saved to {output_path}")


def generate_synthetic_data(n_sessions=500):
    """Generate synthetic grill session data with flare-up events."""
    sessions = []
    for i in range(n_sessions):
        duration_s = np.random.randint(600, 3600)
        n_points = duration_s * 10  # 10 Hz
        thermal_max = 200 + np.cumsum(np.random.normal(0, 0.5, n_points))
        thermal_max = np.clip(thermal_max, 20, 500)
        thermal_grad = np.gradient(thermal_max)
        hot_zones = np.random.randint(0, 8, n_points)
        acoustic = np.random.exponential(50, n_points)
        flame = np.random.randint(0, 50, n_points)
        gas = 50 + np.random.normal(0, 20, n_points)

        # Add flare-up events (5% of sessions have flare-ups)
        flare_events = []
        if np.random.random() < 0.05:
            flare_time = np.random.randint(120, duration_s - 60)
            # Spike thermal and acoustic before flare
            for t in range(max(0, flare_time * 10 - 150), flare_time * 10):
                if t < n_points:
                    thermal_max[t] += np.random.uniform(10, 50)
                    acoustic[t] += np.random.uniform(200, 500)
                    flame[t] = min(255, flame[t] + 100)
            flare_events.append({
                "timestamp_s": flare_time,
                "severity": "major",
                "thermal_spike_c": float(thermal_max[flare_time * 10]),
            })

        sessions.append({
            "session_id": f"synth_{i}",
            "thermal_history": {
                "max_temp": thermal_max.tolist(),
                "gradient": thermal_grad.tolist(),
                "hot_zones": hot_zones.tolist(),
                "acoustic": acoustic.tolist(),
                "flame": flame.tolist(),
                "gas": gas.tolist(),
            },
            "flare_up_events": flare_events,
        })
    return sessions


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train FlareUpNet")
    parser.add_argument("--data", default="/data/flare-events")
    parser.add_argument("--output", default="models/flareup_v1.pth")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--convert-tflite", action="store_true")
    args = parser.parse_args()

    train_model(args.data, args.output, args.epochs, args.batch_size, args.lr)
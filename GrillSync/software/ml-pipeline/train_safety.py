"""
GrillSync — SafetyForecast Training Script
LSTM for cook session safety risk forecasting.

Input:  Cook session features (thermal, gas, probe temps, time)
Output: Safety risk score (0–100) for remaining cook time
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW
from tqdm import tqdm


class SafetyForecastNet(nn.Module):
    def __init__(self, input_size=8, hidden_size=64):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.fc2 = nn.Linear(32, 1)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        out, _ = self.lstm(x)
        out = out[:, -1, :]
        out = self.relu(self.fc1(out))
        out = self.sigmoid(self.fc2(out))
        return out * 100  # 0–100 risk score


class SafetyDataset(Dataset):
    def __init__(self, sessions, seq_len=60):
        self.samples = []
        for session in sessions:
            features = session["features"]
            risk_label = session["risk_label"]
            for i in range(0, len(features) - seq_len, 5):
                window = np.array(features[i:i + seq_len], dtype=np.float32)
                self.samples.append((window, risk_label))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        window, label = self.samples[idx]
        return torch.from_numpy(window), torch.tensor(label, dtype=torch.float32)


def train_model(data_path, output_path, epochs=50, batch_size=64, lr=1e-3):
    print("Generating synthetic safety training data...")
    sessions = generate_synthetic_data(n_sessions=1000)

    dataset = SafetyDataset(sessions)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=4)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = SafetyForecastNet().to(device)
    optimizer = AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    criterion = nn.MSELoss()

    print(f"Training SafetyForecast on {len(dataset)} samples, {epochs} epochs")
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        for batch_x, batch_y in tqdm(loader, desc=f"Epoch {epoch+1}/{epochs}"):
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x).squeeze()
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        print(f"Epoch {epoch+1}: loss={total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), output_path)
    print(f"Model saved to {output_path}")


def generate_synthetic_data(n_sessions=1000):
    sessions = []
    for i in range(n_sessions):
        duration = np.random.randint(120, 600)
        features = []
        base_risk = np.random.uniform(0, 50)
        has_incident = np.random.random() < 0.1
        for t in range(duration):
            temp = 200 + np.random.normal(0, 20)
            gas = 50 + np.random.normal(0, 30)
            acoustic = np.random.exponential(30)
            probe_temp = 20 + (t / duration) * 40
            hot_zones = np.random.randint(0, 5)
            flame = np.random.randint(0, 30)
            # Risk increases if incident
            risk = base_risk + (t / duration) * 20
            if has_incident and t > duration * 0.5:
                risk += (t - duration * 0.5) / duration * 50
            features.append([temp, gas, acoustic, probe_temp, hot_zones,
                            flame, t, duration - t])
        risk_label = min(100, max(0, risk + np.random.normal(0, 5)))
        sessions.append({"features": features, "risk_label": float(risk_label)})
    return sessions


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train SafetyForecast")
    parser.add_argument("--data", default="/data/safety-events")
    parser.add_argument("--output", default="models/safety_forecast_v1.pth")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--lr", type=float, default=1e-3)
    args = parser.parse_args()
    train_model(args.data, args.output, args.epochs, args.batch_size, args.lr)
"""
GrillSync — SmokeNet Training Script
1D-CNN for BBQ smoke quality classification (5-class).

Input:  5-channel time series (30s × 1Hz = 30 timesteps)
        - PM2.5 (µg/m³)
        - VOC index (0–500)
        - Gas resistance (kΩ)
        - CO₂eq (ppm)
        - Smoke opacity (PM1.0/PM2.5 ratio)
Output: 5-class (clean_blue / thin_blue / dirty_white / creosote / no_smoke)
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW
from tqdm import tqdm


# === Model ===
class SmokeNet(nn.Module):
    def __init__(self, input_channels=5, seq_len=30, num_classes=5):
        super().__init__()
        self.conv1 = nn.Conv1d(input_channels, 16, kernel_size=5, padding=2)
        self.conv2 = nn.Conv1d(16, 32, kernel_size=3, padding=1)
        self.pool = nn.MaxPool1d(2)
        self.fc1 = nn.Linear(32 * (seq_len // 4), 32)
        self.fc2 = nn.Linear(32, num_classes)
        self.relu = nn.ReLU()

    def forward(self, x):
        x = self.relu(self.conv1(x))
        x = self.pool(x)
        x = self.relu(self.conv2(x))
        x = self.pool(x)
        x = x.view(x.size(0), -1)
        x = self.relu(self.fc1(x))
        x = self.fc2(x)
        return x


# === Dataset ===
class SmokeDataset(Dataset):
    def __init__(self, sessions, seq_len=30):
        self.samples = []
        quality_map = {"no_smoke": 4, "clean_blue": 0, "thin_blue": 3,
                       "dirty_white": 1, "creosote": 2}
        for session in sessions:
            sensor_data = session["sensor_history"]
            labels = session["labels"]
            for i in range(0, len(sensor_data) - seq_len, 5):
                window = np.array(sensor_data[i:i + seq_len], dtype=np.float32).T
                # Find label for this window
                label_idx = min(i + seq_len // 2, len(labels) - 1)
                quality = labels[label_idx]["quality"]
                self.samples.append((window, quality_map[quality]))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        window, label = self.samples[idx]
        return torch.from_numpy(window), label


# === Training ===
def train_model(data_path, output_path, epochs=50, batch_size=64, lr=1e-3):
    """Train SmokeNet on smoke session data."""
    print("Generating synthetic smoke training data...")
    sessions = generate_synthetic_data(n_sessions=300)

    dataset = SmokeDataset(sessions)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=4)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = SmokeNet().to(device)
    optimizer = AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    criterion = nn.CrossEntropyLoss()

    print(f"Training SmokeNet on {len(dataset)} samples, {epochs} epochs")
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        correct = 0
        total = 0
        for batch_x, batch_y in tqdm(loader, desc=f"Epoch {epoch+1}/{epochs}"):
            batch_x, batch_y = batch_x.to(device), batch_y.to(device)
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
            _, predicted = outputs.max(1)
            correct += (predicted == batch_y).sum().item()
            total += batch_y.size(0)

        acc = 100 * correct / total
        print(f"Epoch {epoch+1}: loss={total_loss/len(loader):.4f} acc={acc:.1f}%")

    torch.save(model.state_dict(), output_path)
    print(f"Model saved to {output_path}")


def generate_synthetic_data(n_sessions=300):
    """Generate synthetic smoke session data."""
    sessions = []
    qualities = ["clean_blue", "thin_blue", "dirty_white", "creosote", "no_smoke"]
    for i in range(n_sessions):
        duration = np.random.randint(300, 3600)
        # Pick a quality profile for this session
        main_quality = np.random.choice(qualities)
        sensor_data = []
        labels = []
        for t in range(duration):
            if main_quality == "clean_blue":
                pm25 = np.random.uniform(30, 80)
                voc = np.random.uniform(50, 150)
                gas = np.random.uniform(20, 40)
                co2 = np.random.uniform(400, 600)
            elif main_quality == "thin_blue":
                pm25 = np.random.uniform(80, 120)
                voc = np.random.uniform(100, 200)
                gas = np.random.uniform(15, 30)
                co2 = np.random.uniform(500, 700)
            elif main_quality == "dirty_white":
                pm25 = np.random.uniform(120, 150)
                voc = np.random.uniform(150, 250)
                gas = np.random.uniform(10, 20)
                co2 = np.random.uniform(600, 900)
            elif main_quality == "creosote":
                pm25 = np.random.uniform(150, 300)
                voc = np.random.uniform(250, 400)
                gas = np.random.uniform(5, 15)
                co2 = np.random.uniform(800, 1200)
            else:  # no_smoke
                pm25 = np.random.uniform(5, 25)
                voc = np.random.uniform(10, 50)
                gas = np.random.uniform(30, 50)
                co2 = np.random.uniform(400, 500)

            pm1 = pm25 * np.random.uniform(0.3, 0.8)
            opacity = pm1 / pm25 if pm25 > 0 else 0
            sensor_data.append([pm25, voc, gas, co2, opacity])
            if t % 30 == 0:
                labels.append({"timestamp_s": t, "quality": main_quality})

        sessions.append({
            "session_id": f"smoke_{i}",
            "sensor_history": sensor_data,
            "labels": labels,
        })
    return sessions


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train SmokeNet")
    parser.add_argument("--data", default="/data/smoke-sessions")
    parser.add_argument("--output", default="models/smoke_v1.pth")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--convert-tflite", action="store_true")
    args = parser.parse_args()
    train_model(args.data, args.output, args.epochs, args.batch_size, args.lr)
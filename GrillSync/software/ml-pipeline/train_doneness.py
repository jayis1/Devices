"""
GrillSync — DonenessNet Training Script
1D-CNN for meat doneness prediction from thermal gradient curves.

Input:  4-channel thermocouple history (90s × 2Hz = 180 timesteps)
        - Tip temperature (°C)
        - Mid temperature (°C)
        - Surface temperature (°C)
        - Ambient (grill) temperature (°C)
Output: 6-class doneness (raw / rare / MR / medium / MW / well)
"""
import argparse
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW
from tqdm import tqdm


# === Model ===
class DonenessNet(nn.Module):
    def __init__(self, input_channels=4, seq_len=180, num_classes=6):
        super().__init__()
        self.conv1 = nn.Conv1d(input_channels, 32, kernel_size=5, padding=2)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=5, padding=2)
        self.pool = nn.MaxPool1d(2)
        self.fc1 = nn.Linear(64 * (seq_len // 4), 64)
        self.fc2 = nn.Linear(64, 16)
        self.fc3 = nn.Linear(16, num_classes)
        self.relu = nn.ReLU()
        self.dropout = nn.Dropout(0.2)

    def forward(self, x):
        # x: (batch, channels, seq_len)
        x = self.relu(self.conv1(x))
        x = self.pool(x)
        x = self.relu(self.conv2(x))
        x = self.pool(x)
        x = x.view(x.size(0), -1)
        x = self.relu(self.fc1(x))
        x = self.dropout(x)
        x = self.relu(self.fc2(x))
        x = self.fc3(x)
        return x


# === Dataset ===
class CookSessionDataset(Dataset):
    def __init__(self, sessions, seq_len=180):
        self.samples = []
        self.seq_len = seq_len
        for session in sessions:
            history = session["probe_history"]
            label = session["label_doneness"]
            # Sliding window over probe history
            for i in range(0, len(history) - seq_len, 10):
                window = history[i:i + seq_len]
                temps = np.array([
                    [w["tip_c"] for w in window],
                    [w["mid_c"] for w in window],
                    [w["surface_c"] for w in window],
                    [w["ambient_c"] for w in window],
                ], dtype=np.float32)
                self.samples.append((temps, label))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        temps, label = self.samples[idx]
        return torch.from_numpy(temps), label


# === Training ===
def train_model(data_path, output_path, epochs=50, batch_size=64, lr=1e-3):
    """Train DonenessNet on cook session data."""
    # Load data (simplified — in production, load from JSON/CSV files)
    # sessions = load_cook_sessions(data_path)
    # For demonstration, generate synthetic data
    print("Generating synthetic training data...")
    sessions = generate_synthetic_data(n_sessions=2000)

    dataset = CookSessionDataset(sessions)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=4)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = DonenessNet().to(device)
    optimizer = AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    criterion = nn.CrossEntropyLoss()

    print(f"Training DonenessNet on {len(dataset)} samples, {epochs} epochs")
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        correct = 0
        total = 0
        for temps, labels in tqdm(loader, desc=f"Epoch {epoch+1}/{epochs}"):
            temps, labels = temps.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(temps)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
            _, predicted = outputs.max(1)
            correct += (predicted == labels).sum().item()
            total += labels.size(0)

        acc = 100 * correct / total
        print(f"Epoch {epoch+1}: loss={total_loss/len(loader):.4f} acc={acc:.1f}%")

    # Save model
    torch.save(model.state_dict(), output_path)
    print(f"Model saved to {output_path}")

    # Convert to TFLite int8
    if args.convert_tflite:
        convert_to_tflite(model, output_path.replace(".pth", ".tflite"))


def convert_to_tflite(model, output_path):
    """Convert PyTorch model to TFLite int8 for ESP32-S3 deployment."""
    print("Converting to TFLite int8...")
    # In production:
    # 1. Export to ONNX: torch.onnx.export(model, ...)
    # 2. Convert ONNX → TFLite: use onnx2tf or ai-edge-litert
    # 3. Quantize to int8: representative dataset calibration
    # 4. Save .tflite file
    print(f"TFLite model would be saved to {output_path}")


def generate_synthetic_data(n_sessions=2000):
    """Generate synthetic cook session data for demonstration."""
    sessions = []
    for i in range(n_sessions):
        meat_type = np.random.randint(0, 5)
        target_temp = [52, 54, 60, 65, 71][np.random.randint(0, 5)]
        duration_s = np.random.randint(600, 3600)
        n_points = duration_s * 2  # 2 Hz
        start_temp = 20.0 + np.random.uniform(-2, 2)
        # Exponential rise toward target
        times = np.arange(n_points) * 0.5
        tip_temps = start_temp + (target_temp - start_temp) * (1 - np.exp(-times / (duration_s / 3)))
        tip_temps += np.random.normal(0, 0.3, n_points)
        mid_temps = tip_temps - np.random.uniform(2, 5, n_points)
        surface_temps = tip_temps + np.random.uniform(5, 15, n_points)
        ambient_temps = 220 + np.random.uniform(-20, 20, n_points)

        history = [
            {"timestamp": t, "tip_c": float(tip), "mid_c": float(mid),
             "surface_c": float(surf), "ambient_c": float(amb)}
            for t, tip, mid, surf, amb in zip(times, tip_temps, mid_temps, surface_temps, ambient_temps)
        ]
        label = np.searchsorted([52, 54, 60, 65, 71], target_temp)

        sessions.append({
            "session_id": f"synth_{i}",
            "meat_type": int(meat_type),
            "probe_history": history,
            "label_doneness": int(min(label, 5)),
        })
    return sessions


# === Evaluation ===
def evaluate_model(model_path, test_data_path):
    """Evaluate DonenessNet on test set."""
    model = DonenessNet()
    model.load_state_dict(torch.load(model_path, weights_only=True))
    model.eval()

    # In production: load test data, compute accuracy, confusion matrix
    print("Evaluation would compute accuracy, precision, recall, F1")
    print("Target: >92% accuracy across 6 doneness classes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train DonenessNet")
    parser.add_argument("--data", default="/data/cook-sessions", help="Data directory")
    parser.add_argument("--output", default="models/doneness_v2.pth", help="Output model path")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--convert-tflite", action="store_true", help="Convert to TFLite int8")
    parser.add_argument("--evaluate", action="store_true", help="Evaluate only")
    args = parser.parse_args()

    if args.evaluate:
        evaluate_model(args.output, args.data)
    else:
        train_model(args.data, args.output, args.epochs, args.batch_size, args.lr)
"""
ErgoFlow — ML Pipeline: RSI Risk Prediction Model Training

RSI-Risk LSTM: Predicts cumulative RSI risk from posture/activity history.
Input: 180-step window of PostureNet outputs + activity vectors
Output: Risk score 0-100

Copyright (c) 2026 jayis1. MIT License.
"""

import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
import numpy as np
import json
from pathlib import Path


# ── Model Definition ──────────────────────────────────────────────────

class RSIRiskNet(nn.Module):
    """
    LSTM-based RSI risk prediction model.

    Input: (batch, seq_len, input_size) — sequence of posture + activity features
    Output: (batch, 1) — risk score 0-100

    Architecture:
        - 2-layer LSTM (128 hidden units)
        - Fully connected head
        - Sigmoid-scaled output for 0-100 risk score
    """

    def __init__(self, input_size=11, hidden_size=128, num_layers=2, dropout=0.3):
        """
        Args:
            input_size: Number of features per timestep
                (5 posture probs + 1 duration + 1 focus + 1 HR + 3 activity one-hot = 11)
            hidden_size: LSTM hidden dimension
            num_layers: Number of LSTM layers
            dropout: Dropout rate
        """
        super(RSIRiskNet, self).__init__()

        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            dropout=dropout if num_layers > 1 else 0,
            batch_first=True,
        )

        self.fc = nn.Sequential(
            nn.Linear(hidden_size, 64),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
        )

    def forward(self, x):
        # x: (batch, seq_len, input_size)
        lstm_out, (h_n, c_n) = self.lstm(x)
        # Use last hidden state
        out = self.fc(h_n[-1])
        # Scale to 0-100
        return torch.sigmoid(out) * 100


# ── Dataset ─────────────────────────────────────────────────────────────

class RSIDataset(Dataset):
    """
    RSI risk dataset.

    Expected format (JSON lines):
    {
        "sequence": [[feat0, feat1, ..., feat10], ...],  // 180 timesteps
        "risk_score": 45.0,  // target 0-100
        "duration_hours": 8
    }
    """

    def __init__(self, data_path: str):
        self.sequences = []
        self.risk_scores = []
        self._load_data(Path(data_path))

    def _load_data(self, path: Path):
        with open(path) as f:
            for line in f:
                record = json.loads(line.strip())
                self.sequences.append(np.array(record["sequence"], dtype=np.float32))
                self.risk_scores.append(float(record["risk_score"]))

    def __len__(self):
        return len(self.sequences)

    def __getitem__(self, idx):
        return (
            torch.FloatTensor(self.sequences[idx]),
            torch.FloatTensor([self.risk_scores[idx]]),
        )


# ── Training ────────────────────────────────────────────────────────────

def train_rsi_net(
    data_path: str = "data/rsi_data.jsonl",
    epochs: int = 50,
    batch_size: int = 32,
    learning_rate: float = 0.0005,
    output_dir: str = "models",
):
    """Train RSI risk prediction model."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training RSI-Risk Net on: {device}")

    # Load data
    dataset = RSIDataset(data_path)
    print(f"Dataset size: {len(dataset)} sequences")

    # Train/val split
    val_size = int(0.2 * len(dataset))
    train_size = len(dataset) - val_size
    train_dataset, val_dataset = torch.utils.data.random_split(
        dataset, [train_size, val_size]
    )

    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False)

    # Model
    model = RSIRiskNet().to(device)
    criterion = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=learning_rate, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=5, factor=0.5)

    best_val_loss = float('inf')

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for sequences, targets in train_loader:
            sequences, targets = sequences.to(device), targets.to(device)
            optimizer.zero_grad()
            predictions = model(sequences)
            loss = criterion(predictions, targets)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        model.eval()
        val_loss = 0
        with torch.no_grad():
            for sequences, targets in val_loader:
                sequences, targets = sequences.to(device), targets.to(device)
                predictions = model(sequences)
                loss = criterion(predictions, targets)
                val_loss += loss.item()
        val_loss /= len(val_loader)

        scheduler.step(val_loss)

        if (epoch + 1) % 5 == 0:
            print(f"Epoch {epoch+1:3d}: train_loss={train_loss:.4f}, val_loss={val_loss:.4f}")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save({
                "epoch": epoch + 1,
                "model_state_dict": model.state_dict(),
                "val_loss": val_loss,
            }, output_path / "rsi_risk_net_best.pt")

    # Save final model
    torch.save({
        "epoch": epochs,
        "model_state_dict": model.state_dict(),
    }, output_path / "rsi_risk_net_final.pt")

    # Export ONNX
    dummy_input = torch.randn(1, 180, 11)
    torch.onnx.export(
        model, dummy_input,
        str(output_path / "rsi_risk_net.onnx"),
        input_names=["sequence"],
        output_names=["risk_score"],
        dynamic_axes={"sequence": {0: "batch"}, "risk_score": {0: "batch"}},
    )

    param_count = sum(p.numel() for p in model.parameters())
    print(f"\nRSI-Risk Net trained! Parameters: {param_count:,}")
    print(f"Model size: {param_count * 4 / 1024:.1f} KB (float32)")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Train RSI Risk Net")
    parser.add_argument("--data", default="data/rsi_data.jsonl")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--output", default="models")
    args = parser.parse_args()
    train_rsi_net(data_path=args.data, epochs=args.epochs, output_dir=args.output)
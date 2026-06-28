"""
PestSync ML Pipeline — Activity Pattern LSTM
software/ml-pipeline/train_activity_lstm.py

Trains LSTM to predict pest activity patterns (diurnal/nocturnal/crepuscular)
from 7-day hourly detection counts. Used for adaptive deterrent scheduling.
"""
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader

SEQ_LENGTH = 168  # 7 days × 24 hours
NUM_FEATURES = 1  # hourly detection count
NUM_CLASSES = 3    # diurnal, nocturnal, crepuscular
HIDDEN_SIZE = 64
BATCH_SIZE = 64
EPOCHS = 100
LEARNING_RATE = 0.001
DATA_PATH = "data/activity_logs.npy"
LABELS_PATH = "data/activity_labels.npy"
MODEL_OUTPUT = "models/activity_lstm.pt"


class ActivityDataset(Dataset):
    def __init__(self, sequences, labels):
        self.sequences = sequences
        self.labels = labels

    def __len__(self):
        return len(self.sequences)

    def __getitem__(self, idx):
        seq = torch.FloatTensor(self.sequences[idx])
        label = torch.LongTensor([self.labels[idx]])
        return seq, label


class ActivityLSTM(nn.Module):
    def __init__(self):
        super().__init__()
        self.lstm = nn.LSTM(
            input_size=NUM_FEATURES,
            hidden_size=HIDDEN_SIZE,
            num_layers=2,
            batch_first=True,
            dropout=0.3,
        )
        self.fc1 = nn.Linear(HIDDEN_SIZE, 32)
        self.fc2 = nn.Linear(32, NUM_CLASSES)
        self.relu = nn.ReLU()
        self.dropout = nn.Dropout(0.2)

    def forward(self, x):
        # x: (batch, seq_len, features)
        _, (hidden, _) = self.lstm(x)
        x = self.relu(self.fc1(hidden[-1]))
        x = self.dropout(x)
        x = self.fc2(x)
        return x

    def get_peak_hours(self, sequence):
        """Extract peak activity hours from a 7-day sequence."""
        # Reshape to 7×24 and average per hour
        hourly_avg = np.mean(sequence.reshape(7, 24), axis=0)
        peak_hours = np.argsort(hourly_avg)[-3:][::-1].tolist()

        # Classify pattern
        nocturnal = sum(hourly_avg[[0, 1, 2, 3, 22, 23]])
        diurnal = sum(hourly_avg[[9, 10, 11, 12, 13, 14]])
        crepuscular = sum(hourly_avg[[18, 19, 20, 21]])

        if nocturnal > diurnal and nocturnal > crepuscular:
            pattern = "nocturnal"
        elif crepuscular > diurnal:
            pattern = "crepuscular"
        else:
            pattern = "diurnal"

        return pattern, peak_hours


def generate_synthetic_data(n_samples=10000):
    """Generate synthetic activity data using agent-based simulation."""
    np.random.seed(42)
    sequences = []
    labels = []

    for _ in range(n_samples):
        pattern = np.random.choice([0, 1, 2], p=[0.3, 0.4, 0.3])

        # Base activity profile per pattern
        hourly_profile = np.zeros(24)
        if pattern == 0:  # diurnal (ants, flies)
            hourly_profile[9:15] = np.random.uniform(0.5, 1.0, 6)
        elif pattern == 1:  # nocturnal (rodents)
            hourly_profile[0:5] = np.random.uniform(0.5, 1.0, 5)
            hourly_profile[22:24] = np.random.uniform(0.3, 0.8, 2)
        else:  # crepuscular (cockroaches)
            hourly_profile[18:22] = np.random.uniform(0.5, 1.0, 4)

        # Generate 7 days of hourly counts
        seq = []
        for day in range(7):
            for hour in range(24):
                base = hourly_profile[hour]
                noise = np.random.poisson(max(base * 5, 0.1))
                seq.append(noise)

        sequences.append(seq)
        labels.append(pattern)

    return np.array(sequences, dtype=np.float32), np.array(labels, dtype=np.int64)


def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on: {device}")

    # Load or generate data
    try:
        sequences = np.load(DATA_PATH)
        labels = np.load(LABELS_PATH)
        print(f"Loaded {len(sequences)} activity logs")
    except FileNotFoundError:
        print("Generating synthetic activity data...")
        sequences, labels = generate_synthetic_data(10000)
        os.makedirs("data", exist_ok=True)
        np.save(DATA_PATH, sequences)
        np.save(LABELS_PATH, labels)

    # Normalize
    max_val = sequences.max()
    sequences = sequences / max_val

    # Split
    split = int(0.8 * len(sequences))
    train_ds = ActivityDataset(sequences[:split], labels[:split])
    val_ds = ActivityDataset(sequences[split:], labels[split:])
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False)

    model = ActivityLSTM().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=LEARNING_RATE)

    best_acc = 0.0
    for epoch in range(EPOCHS):
        model.train()
        for batch_seqs, batch_labels in train_loader:
            batch_seqs = batch_seqs.to(device)
            batch_labels = batch_labels.squeeze().to(device)
            optimizer.zero_grad()
            outputs = model(batch_seqs)
            loss = criterion(outputs, batch_labels)
            loss.backward()
            optimizer.step()

        # Validate
        model.eval()
        correct = 0
        total = 0
        with torch.no_grad():
            for batch_seqs, batch_labels in val_loader:
                batch_seqs = batch_seqs.to(device)
                batch_labels = batch_labels.squeeze().to(device)
                outputs = model(batch_seqs)
                _, predicted = torch.max(outputs, 1)
                total += batch_labels.size(0)
                correct += (predicted == batch_labels).sum().item()

        val_acc = 100 * correct / total
        if val_acc > best_acc:
            best_acc = val_acc
            os.makedirs("models", exist_ok=True)
            torch.save(model.state_dict(), MODEL_OUTPUT)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} - Val Acc: {val_acc:.2f}%")

    print(f"\n✅ Best validation accuracy: {best_acc:.2f}%")
    print(f"Model saved to {MODEL_OUTPUT}")


if __name__ == "__main__":
    import os
    train()
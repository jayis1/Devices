"""
TrailSync — Injury Risk Predictor Training Script

12-class injury risk model using gait metrics, HRV trends, training load,
vertical gain, terrain type, and 7-day training history. Produces a 7-day
injury risk forecast for each injury class.

Architecture: Multi-input model (time-series gait + HRV + static features)
- Gait LSTM branch: 7-day gait metric history (7 × 6 features)
- HRV LSTM branch: 7-day HRV history (7 × 4 features)
- Static features: age, weight, weekly mileage, vertical gain, terrain mix
- Output: 12 injury risk scores (0-100) + 1 overall risk + recommendations

SPDX-License-Identifier: MIT
"""
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
import json

# ─── Configuration ──────────────────────────────────────────────────────

INJURY_CLASSES = [
    "IT band syndrome", "plantar fasciitis", "Achilles tendinopathy",
    "stress fracture", "shin splints", "runner's knee",
    "ankle sprain", "hamstring strain", "hip flexor strain",
    "calf strain", "IT band friction", "patellar tendinopathy"
]
GAIT_FEATURES = 6   # cadence, contact_time, vert_osc, impact, pronation, stride
HRV_FEATURES = 4    # hr, hrv_rmssd, spo2, skin_temp
STATIC_FEATURES = 6 # age, weight, weekly_mileage, vert_gain, acute_chronic_ratio, terrain_risk
HISTORY_DAYS = 7
BATCH_SIZE = 32
EPOCHS = 40
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"


# ─── Dataset ────────────────────────────────────────────────────────────

class InjuryRiskDataset(Dataset):
    def __init__(self, gait_history, hrv_history, static_features, risk_labels):
        self.gait = torch.FloatTensor(gait_history)           # (N, 7, 6)
        self.hrv = torch.FloatTensor(hrv_history)               # (N, 7, 4)
        self.static = torch.FloatTensor(static_features)        # (N, 6)
        self.labels = torch.FloatTensor(risk_labels)            # (N, 12)

    def __len__(self):
        return len(self.labels)

    def __getitem__(self, idx):
        return self.gait[idx], self.hrv[idx], self.static[idx], self.labels[idx]


# ─── Model ──────────────────────────────────────────────────────────────

class InjuryRiskModel(nn.Module):
    """Multi-input model for injury risk prediction."""
    def __init__(self, gait_dim=6, hrv_dim=4, static_dim=6,
                 hidden_dim=64, num_injuries=12):
        super().__init__()

        # Gait LSTM branch
        self.gait_lstm = nn.LSTM(gait_dim, hidden_dim, num_layers=2,
                                  batch_first=True, bidirectional=True, dropout=0.2)

        # HRV LSTM branch
        self.hrv_lstm = nn.LSTM(hrv_dim, hidden_dim, num_layers=2,
                                 batch_first=True, bidirectional=True, dropout=0.2)

        # Static features MLP
        self.static_mlp = nn.Sequential(
            nn.Linear(static_dim, 32),
            nn.ReLU(),
            nn.Dropout(0.2),
        )

        # Combined injury risk head
        combined_dim = hidden_dim * 4 + 32  # gait*2 + hrv*2 + static
        self.risk_head = nn.Sequential(
            nn.Linear(combined_dim, 128),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, num_injuries),
            nn.Sigmoid(),  # Output 0-1 (risk percentage)
        )

    def forward(self, gait, hrv, static):
        # Gait branch
        gait_out, _ = self.gait_lstm(gait)
        gait_feat = gait_out[:, -1, :]  # Last timestep

        # HRV branch
        hrv_out, _ = self.hrv_lstm(hrv)
        hrv_feat = hrv_out[:, -1, :]

        # Static branch
        static_feat = self.static_mlp(static)

        # Concatenate and predict
        combined = torch.cat([gait_feat, hrv_feat, static_feat], dim=-1)
        risk_scores = self.risk_head(combined)  # (batch, 12) in [0, 1]

        return risk_scores


# ─── Training ───────────────────────────────────────────────────────────

def train_injury_model(train_loader, val_loader, epochs=EPOCHS):
    model = InjuryRiskModel().to(DEVICE)
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
    criterion = nn.MSELoss()  # Regression on risk scores

    best_val_loss = float('inf')

    for epoch in range(epochs):
        model.train()
        train_loss = 0
        for gait, hrv, static, labels in train_loader:
            gait, hrv, static, labels = (gait.to(DEVICE), hrv.to(DEVICE),
                                          static.to(DEVICE), labels.to(DEVICE))
            pred = model(gait, hrv, static)
            loss = criterion(pred, labels)

            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item()

        scheduler.step()

        # Validation
        model.eval()
        val_loss = 0
        with torch.no_grad():
            for gait, hrv, static, labels in val_loader:
                gait, hrv, static, labels = (gait.to(DEVICE), hrv.to(DEVICE),
                                              static.to(DEVICE), labels.to(DEVICE))
                pred = model(gait, hrv, static)
                val_loss += criterion(pred, labels).item()

        avg_val = val_loss / len(val_loader)
        if avg_val < best_val_loss:
            best_val_loss = avg_val
            torch.save(model.state_dict(), "injury_risk_model_best.pt")

        if (epoch + 1) % 5 == 0:
            print(f"Epoch {epoch+1}/{epochs} — Train: {train_loss/len(train_loader):.4f} "
                  f"Val: {avg_val:.4f}")

    return model


# ─── Synthetic Data ────────────────────────────────────────────────────

def generate_synthetic_data(n_samples=5000):
    np.random.seed(42)
    gait = np.random.randn(n_samples, HISTORY_DAYS, GAIT_FEATURES).astype(np.float32)
    hrv = np.random.randn(n_samples, HISTORY_DAYS, HRV_FEATURES).astype(np.float32)
    static = np.random.randn(n_samples, STATIC_FEATURES).astype(np.float32)
    labels = np.random.uniform(0, 0.5, (n_samples, len(INJURY_CLASSES))).astype(np.float32)

    # Make labels correlated with features
    # High impact → high stress fracture risk
    labels[:, 3] = np.clip(gait[:, :, 3].mean(axis=1) / 500, 0, 1)
    # High pronation → high plantar fasciitis risk
    labels[:, 1] = np.clip(gait[:, :, 4].mean(axis=1) / 20, 0, 1)
    # High asymmetry → high IT band risk
    labels[:, 0] = np.clip(np.abs(gait[:, :, 0]).mean(axis=1) / 0.15, 0, 1)
    # Low HRV → higher overall injury risk
    hrv_avg = hrv[:, :, 1].mean(axis=1)
    risk_boost = np.clip((50 - hrv_avg) / 50, 0, 0.3)
    labels += risk_boost[:, None]

    labels = np.clip(labels, 0, 1)
    return gait, hrv, static, labels


# ─── Recommendation Engine ──────────────────────────────────────────────

def generate_recommendations(risk_scores, gait_metrics, training_load):
    """Generate personalized recommendations based on injury risk scores."""
    recs = []
    for i, (injury, risk) in enumerate(zip(INJURY_CLASSES, risk_scores)):
        if risk > 0.6:
            recs.append(f"HIGH RISK ({risk:.0%}): {injury} — reduce training volume 30% and see a sports medicine professional")
        elif risk > 0.4:
            recs.append(f"MODERATE RISK ({risk:.0%}): {injury} — add preventive exercises and monitor closely")

    # Training load recommendations
    if training_load > 1.3:
        recs.append(f"Acute:chronic workload ratio is {training_load:.1f} — reduce volume to avoid overuse injuries")
    elif training_load > 1.0:
        recs.append(f"Acute:chronic workload ratio is {training_load:.1f} — maintain current volume, don't increase")

    return recs[:5]  # Top 5 recommendations


if __name__ == "__main__":
    print("TrailSync Injury Risk Predictor Training")
    print("=" * 50)

    gait, hrv, static, labels = generate_synthetic_data()
    indices = np.arange(len(labels))
    train_idx, val_idx = train_test_split(indices, test_size=0.2)

    train_ds = InjuryRiskDataset(gait[train_idx], hrv[train_idx],
                                  static[train_idx], labels[train_idx])
    val_ds = InjuryRiskDataset(gait[val_idx], hrv[val_idx],
                                static[val_idx], labels[val_idx])

    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE)

    print(f"\nTraining on {DEVICE}...")
    model = train_injury_model(train_loader, val_loader, epochs=EPOCHS)

    # Test prediction
    model.eval()
    sample_gait = torch.randn(1, HISTORY_DAYS, GAIT_FEATURES).to(DEVICE)
    sample_hrv = torch.randn(1, HISTORY_DAYS, HRV_FEATURES).to(DEVICE)
    sample_static = torch.randn(1, STATIC_FEATURES).to(DEVICE)
    with torch.no_grad():
        risk = model(sample_gait, sample_hrv, sample_static)

    print("\nSample Injury Risk Forecast:")
    for injury, score in zip(INJURY_CLASSES, risk[0].cpu().numpy()):
        bar = "█" * int(score * 20) + "░" * (20 - int(score * 20))
        print(f"  {injury:25s} [{bar}] {score:.1%}")

    recs = generate_recommendations(risk[0].cpu().numpy(),
                                     gait_metrics=None, training_load=1.1)
    print("\nRecommendations:")
    for rec in recs:
        print(f"  • {rec}")

    print("\nTraining complete! Model saved to injury_risk_model_best.pt")
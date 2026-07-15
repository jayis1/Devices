"""
LawnSync ML Pipeline — DroughtNet & FertScheduler

DroughtNet: 1D-CNN on NDVI time-series + spatial NDVI map (4-class)
FertScheduler: XGBoost fertilization timing optimizer
"""

import os
import numpy as np
import torch
import torch.nn as nn
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, mean_absolute_error

MODEL_SAVE_DIR = "./models"
os.makedirs(MODEL_SAVE_DIR, exist_ok=True)


# ---------------------------------------------------------------------------
# DroughtNet — NDVI Drought Stress Classifier (4-class)
# ---------------------------------------------------------------------------

class DroughtNet(nn.Module):
    """1D-CNN for drought stress classification from NDVI time-series.

    Input:
        ndvi_series: (batch, 7) — 7-day NDVI trend
        ndvi_map: (batch, 1, 64, 64) — spatial NDVI map
    Output:
        logits: (batch, 4) — [healthy, mild, moderate, severe]
    """

    def __init__(self):
        super().__init__()
        # 1D CNN for temporal NDVI series
        self.temporal = nn.Sequential(
            nn.Conv1d(1, 16, 3, padding=1),
            nn.ReLU(),
            nn.Conv1d(16, 32, 3, padding=1),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
            nn.Flatten(),
        )
        # Small CNN for spatial NDVI map
        self.spatial = nn.Sequential(
            nn.Conv2d(1, 16, 3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Conv2d(16, 32, 3, padding=1),
            nn.ReLU(),
            nn.AdaptiveAvgPool2d(1),
            nn.Flatten(),
        )
        # Fusion + classifier
        self.classifier = nn.Sequential(
            nn.Linear(32 + 32, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, 4),
        )

    def forward(self, ndvi_series, ndvi_map):
        t = self.temporal(ndvi_series.unsqueeze(1))  # (batch, 32)
        s = self.spatial(ndvi_map)                    # (batch, 32)
        combined = torch.cat([t, s], dim=1)           # (batch, 64)
        return self.classifier(combined)               # (batch, 4)


def train_droughtnet():
    """Train DroughtNet on synthetic NDVI data."""
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[DroughtNet] Training on {device}")

    # Synthetic data: 8000 samples
    n = 8000
    ndvi_series = torch.randn(n, 7)
    ndvi_maps = torch.randn(n, 1, 64, 64)
    labels = torch.randint(0, 4, (n,))

    # Split
    split = int(0.8 * n)
    train_s, train_m, train_l = ndvi_series[:split], ndvi_maps[:split], labels[:split]
    val_s, val_m, val_l = ndvi_series[split:], ndvi_maps[split:], labels[split:]

    model = DroughtNet().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=0.001)

    epochs = 50
    batch_size = 64

    best_acc = 0
    for epoch in range(epochs):
        model.train()
        for i in range(0, len(train_s), batch_size):
            bs = train_s[i:i+batch_size].to(device)
            bm = train_m[i:i+batch_size].to(device)
            bl = train_l[i:i+batch_size].to(device)
            optimizer.zero_grad()
            out = model(bs, bm)
            loss = criterion(out, bl)
            loss.backward()
            optimizer.step()

        # Validate
        model.eval()
        with torch.no_grad():
            preds = model(val_s.to(device), val_m.to(device)).argmax(1)
            acc = (preds == val_l.to(device)).float().mean().item()

        if acc > best_acc:
            best_acc = acc
            torch.save(model.state_dict(),
                       os.path.join(MODEL_SAVE_DIR, "droughtnet.pth"))

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} — Val Acc: {acc:.4f}")

    print(f"[DroughtNet] Best accuracy: {best_acc:.4f}")
    return best_acc


# ---------------------------------------------------------------------------
# FertScheduler — XGBoost Fertilization Timing Optimizer
# ---------------------------------------------------------------------------

def train_fertscheduler():
    """Train XGBoost model for fertilization timing prediction.

    Input: soil N/P/K, grass type, growth stage, 14-day weather,
           last fert date, soil temp, moisture
    Output: Days until optimal fertilization window
    """
    print("[FertScheduler] Training XGBoost model")

    # Synthetic training data: 15000 samples
    n = 15000
    rng = np.random.RandomState(42)
    features = []
    targets = []

    for _ in range(n):
        nitrogen = rng.uniform(10, 80)      # mg/kg
        phosphorus = rng.uniform(5, 40)     # mg/kg
        potassium = rng.uniform(50, 200)    # mg/kg
        grass_type = rng.randint(0, 5)      # 6 grass types
        growth_stage = rng.randint(0, 4)    # dormant, green-up, active, dormant-prep
        soil_temp = rng.uniform(5, 30)      # °C
        soil_moisture = rng.uniform(10, 35) # % VWC
        rain_7d = rng.uniform(0, 50)        # mm in next 7 days
        temp_avg = rng.uniform(10, 35)      # avg temp next 14 days
        days_since_fert = rng.randint(0, 90) # days
        lawn_area = rng.uniform(50, 500)    # m²

        features.append([nitrogen, phosphorus, potassium, grass_type,
                         growth_stage, soil_temp, soil_moisture,
                         rain_7d, temp_avg, days_since_fert, lawn_area])

        # Target: days until optimal fertilization window
        # Heuristic: if N low + soil_temp 15-25 + rain <10mm in 3 days → soon
        if nitrogen < 25 and 15 <= soil_temp <= 25 and days_since_fert > 30:
            target = rng.uniform(0, 5)
        elif nitrogen < 35 and days_since_fert > 45:
            target = rng.uniform(3, 14)
        elif nitrogen > 60:
            target = rng.uniform(30, 60)
        else:
            target = rng.uniform(7, 21)

        targets.append(target)

    X = np.array(features)
    y = np.array(targets)

    X_train, X_val, y_train, y_val = train_test_split(X, y, test_size=0.2,
                                                       random_state=42)

    model = xgb.XGBRegressor(
        n_estimators=500,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        random_state=42,
    )
    model.fit(X_train, y_train, eval_set=[(X_val, y_val)],
              verbose=False)

    val_pred = model.predict(X_val)
    mae = mean_absolute_error(y_val, val_pred)
    print(f"[FertScheduler] Validation MAE: {mae:.2f} days")

    model.save_model(os.path.join(MODEL_SAVE_DIR, "fert_scheduler.json"))

    # Feature importance
    importance = model.feature_importances_
    feat_names = ["N", "P", "K", "grass_type", "growth_stage", "soil_temp",
                  "soil_moisture", "rain_7d", "temp_avg", "days_since_fert", "lawn_area"]
    print("[FertScheduler] Feature importance:")
    for name, imp in sorted(zip(feat_names, importance), key=lambda x: -x[1]):
        print(f"  {name}: {imp:.4f}")

    return mae


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("=" * 60)
    print("LawnSync ML Pipeline — DroughtNet & FertScheduler")
    print("=" * 60)
    train_droughtnet()
    print()
    train_fertscheduler()
    print("\nDone!")
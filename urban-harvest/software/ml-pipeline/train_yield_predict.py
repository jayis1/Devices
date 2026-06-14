"""
UrbanHarvest - Yield Prediction Model Training
LSTM sequence model with attention for harvest date and yield estimation
Uses historical growing data, cumulative light/water/nutrients, weather

Input features:
  - Plant type (one-hot encoded)
  - Days since planting
  - Cumulative light hours (PAR integral)
  - Cumulative water volume (ml)
  - Average soil EC (mS/cm)
  - Mean temperature (°C)
  - Total nutrient A/B doses (ml)
  - Disease event count
  - Current plant health index

Output:
  - Days to harvest
  - Estimated yield (grams)
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from sklearn.preprocessing import StandardScaler
import joblib

# ========== CONFIGURATION ==========

SEQ_LENGTH = 90        # 90-day growing season window
INPUT_FEATURES = 10    # Number of input features per timestep
HIDDEN_SIZE = 64
NUM_LAYERS = 2
DROPOUT = 0.2
BATCH_SIZE = 32
EPOCHS = 100
LEARNING_RATE = 1e-3

MODEL_DIR = os.getenv("MODEL_DIR", "./models")

PLANT_TYPES = ["generic", "tomato", "basil", "lettuce", "pepper",
               "mint", "microgreens", "cucumber", "strawberry", "spinach"]

# ========== SYNTHETIC DATA GENERATION ==========
# In production, load from database of real growing histories
# Here we generate realistic synthetic training data

def generate_synthetic_data(n_samples=5000):
    """Generate realistic synthetic growing season data for training"""

    X = np.zeros((n_samples, SEQ_LENGTH, INPUT_FEATURES))
    y_days = np.zeros(n_samples)      # Days to harvest
    y_yield = np.zeros(n_samples)      # Estimated yield in grams

    for i in range(n_samples):
        plant_idx = np.random.randint(0, len(PLANT_TYPES))
        plant_type = PLANT_TYPES[plant_idx]

        # Typical growing season lengths by plant type
        season_lengths = {
            "generic": (50, 90), "tomato": (60, 120), "basil": (30, 60),
            "lettuce": (30, 50), "pepper": (70, 110), "mint": (30, 45),
            "microgreens": (7, 21), "cucumber": (50, 80),
            "strawberry": (60, 90), "spinach": (30, 45),
        }

        # Typical yields by plant type (grams per plant)
        typical_yields = {
            "generic": (100, 500), "tomato": (500, 3000), "basil": (30, 100),
            "lettuce": (100, 300), "pepper": (200, 1000), "mint": (20, 80),
            "microgreens": (20, 100), "cucumber": (300, 1500),
            "strawberry": (100, 500), "spinach": (50, 200),
        }

        min_season, max_season = season_lengths.get(plant_type, (50, 90))
        actual_days = np.random.randint(min_season, max_season + 1)
        min_yield, max_yield = typical_yields.get(plant_type, (100, 500))

        # Base yield with noise
        base_yield = np.random.uniform(min_yield, max_yield)

        for day in range(SEQ_LENGTH):
            # One-hot plant type (10 classes)
            features = np.zeros(INPUT_FEATURES)
            features[plant_idx] = 1.0  # Plant type one-hot

            # Days since planting (normalized to 0-1)
            if day < actual_days:
                features[10 - 3] = day / actual_days  # days_since_planting (norm)

                # Cumulative light (PAR integral, increases with time)
                par_daily = np.random.normal(400, 150)  # µmol/m²/s average
                cum_light = par_daily * day / 1000.0  # Mol/m² (simplified)
                features[10 - 2] = cum_light / 50.0  # Normalized

                # Cumulative water (ml, increases with time)
                daily_water = np.random.normal(200, 50)  # ml per day
                cum_water = daily_water * day / 1000.0
                features[10 - 1] = cum_water / 20.0  # Normalized
            else:
                # After harvest: zero out
                features[10 - 3] = 1.0
                features[10 - 2] = 0.8
                features[10 - 1] = 0.6

            X[i, day] = features

        # Target: days remaining to harvest, estimated yield
        y_days[i] = actual_days
        y_yield[i] = base_yield

    return X, y_days, y_yield


# ========== DATASET ==========

class YieldDataset(Dataset):
    def __init__(self, X, y_days, y_yield):
        self.X = torch.FloatTensor(X)
        self.y_days = torch.FloatTensor(y_days).unsqueeze(1)
        self.y_yield = torch.FloatTensor(y_yield).unsqueeze(1)

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        return self.X[idx], self.y_days[idx], self.y_yield[idx]


# ========== MODEL ==========

class YieldPredictor(nn.Module):
    """
    LSTM-based yield prediction model with attention mechanism.
    Processes 90-day sequence of growing features.
    Predicts: days_to_harvest, estimated_yield_g
    """

    def __init__(self, input_size=INPUT_FEATURES, hidden_size=HIDDEN_SIZE,
                 num_layers=NUM_LAYERS, dropout=DROPOUT):
        super().__init__()

        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout if num_layers > 1 else 0,
            bidirectional=False,
        )

        # Attention layer
        self.attention = nn.Sequential(
            nn.Linear(hidden_size, hidden_size // 2),
            nn.Tanh(),
            nn.Linear(hidden_size // 2, 1),
        )

        # Output heads
        self.days_head = nn.Sequential(
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(32, 1),
        )

        self.yield_head = nn.Sequential(
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(32, 1),
            nn.ReLU(),  # Yield is always positive
        )

    def forward(self, x):
        # LSTM encoding
        lstm_out, (h_n, c_n) = self.lstm(x)
        # lstm_out: (batch, seq_len, hidden_size)

        # Attention weights
        attn_weights = self.attention(lstm_out)  # (batch, seq_len, 1)
        attn_weights = torch.softmax(attn_weights, dim=1)

        # Weighted sum
        context = torch.sum(attn_weights * lstm_out, dim=1)  # (batch, hidden_size)

        # Predict
        days_pred = self.days_head(context)
        yield_pred = self.yield_head(context)

        return days_pred, yield_pred


# ========== TRAINING ==========

def train_model():
    """Train the yield prediction model"""

    print("Generating synthetic training data...")
    X, y_days, y_yield = generate_synthetic_data(n_samples=5000)

    # Split train/val
    split = int(0.8 * len(X))
    X_train, X_val = X[:split], X[split:]
    yd_train, yd_val = y_days[:split], y_days[split:]
    yy_train, yy_val = y_yield[:split], y_yield[split:]

    train_dataset = YieldDataset(X_train, yd_train, yy_train)
    val_dataset = YieldDataset(X_val, yd_val, yy_val)

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)

    # Model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = YieldPredictor().to(device)

    # Loss and optimizer
    criterion_days = nn.MSELoss()
    criterion_yield = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=LEARNING_RATE)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)

    best_val_loss = float('inf')

    for epoch in range(EPOCHS):
        # Training
        model.train()
        train_loss = 0
        for batch_x, batch_yd, batch_yy in train_loader:
            batch_x = batch_x.to(device)
            batch_yd = batch_yd.to(device)
            batch_yy = batch_yy.to(device)

            pred_days, pred_yield = model(batch_x)
            loss = criterion_days(pred_days, batch_yd) + 0.01 * criterion_yield(pred_yield, batch_yy)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        # Validation
        model.eval()
        val_loss = 0
        days_errors = []
        yield_errors = []
        with torch.no_grad():
            for batch_x, batch_yd, batch_yy in val_loader:
                batch_x = batch_x.to(device)
                batch_yd = batch_yd.to(device)
                batch_yy = batch_yy.to(device)

                pred_days, pred_yield = model(batch_x)
                loss = criterion_days(pred_days, batch_yd) + 0.01 * criterion_yield(pred_yield, batch_yy)
                val_loss += loss.item()

                days_errors.extend((pred_days.cpu() - batch_yd.cpu()).abs().numpy().flatten())
                yield_errors.extend((pred_yield.cpu() - batch_yy.cpu()).abs().numpy().flatten())

        val_loss /= len(val_loader)
        scheduler.step(val_loss)

        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Train Loss: {train_loss:.4f} | Val Loss: {val_loss:.4f}")
            print(f"  Days MAE: {np.mean(days_errors):.1f} days | Yield MAE: {np.mean(yield_errors):.0f} g")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            os.makedirs(MODEL_DIR, exist_ok=True)
            torch.save(model.state_dict(), os.path.join(MODEL_DIR, "yield_predictor.pt"))
            print(f"  ✓ Best model saved (val_loss: {val_loss:.4f})")

    return model


# ========== MAIN ==========

if __name__ == "__main__":
    print("=" * 60)
    print("UrbanHarvest — Yield Prediction Model Training")
    print("=" * 60)

    model = train_model()

    print("\n" + "=" * 60)
    print("Training complete!")
    print(f"  Model saved: {MODEL_DIR}/yield_predictor.pt")
    print("=" * 60)
    print("\nDeployment:")
    print("  1. Load model in FastAPI backend (ml_inference.py)")
    print("  2. Call model with plant growing history")
    print("  3. Return predicted harvest date + estimated yield to mobile app")
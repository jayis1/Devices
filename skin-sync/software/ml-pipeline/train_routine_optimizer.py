"""
SkinSync Skincare Routine Optimizer — Personalized Product Efficacy Model

Learns which skincare products and ingredients actually improve each user's
skin by correlating product usage (from dispenser logs) with skin condition
scores (from scanner) over time. Recommends routine adjustments.

Architecture:
  - Gradient Boosted Trees (LightGBM/XGBoost) for ingredient efficacy
  - Collaborative filtering for product recommendations
  - Ingredient interaction model (retinol + AHA irritation, etc.)

Inputs: 90 days of scan results + dispense events + UV exposure
Outputs: routine recommendations (add/remove/adjust products, amounts, timing)
"""

import os
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
from torch.optim import AdamW

DAYS         = 90
FEATURES_PER_DAY = 10  # condition_score, hydration, pigmentation, product amounts, UV dose
BATCH_SIZE   = 32
EPOCHS       = 50
LR           = 1e-3
DEVICE       = "cuda" if torch.cuda.is_available() else "cpu"
MODEL_SAVE   = os.path.join(os.path.dirname(__file__), "routine_optimizer.pt")

# Ingredient interaction matrix (1=synergy, -1=irritation, 0=neutral)
INGREDIENT_INTERACTIONS = {
    ("retinol", "aha_bha"): -1,    # irritation
    ("retinol", "vitamin_c"): -1,  # use at different times
    ("retinol", "niacinamide"): 1, # synergy (barrier support)
    ("vitamin_c", "niacinamide"): 0, # historically thought negative, now neutral
    ("spf", "retinol"): 0,         # use at different times (AM/PM)
    ("hyaluronic_acid", "any"): 1, # generally beneficial
    ("aha_bha", "niacinamide"): 1, # pH balancing
}


class RoutineOptimizer(nn.Module):
    """Predicts skin condition improvement from routine changes.

    Input: 90-day history (condition scores + product usage)
    Output: recommended routine adjustments
      - product_scores: efficacy score per product (0-1)
      - adjustment: add/remove/maintain per product
      - optimal_amount: recommended amount (mg)
    """
    def __init__(self, n_products=4):
        super().__init__()
        self.lstm = nn.LSTM(FEATURES_PER_DAY, 128, num_layers=2,
                            batch_first=True, dropout=0.2)
        self.product_head = nn.Sequential(
            nn.Linear(128, 64), nn.ReLU(),
            nn.Linear(64, n_products), nn.Sigmoid()
        )
        self.adjustment_head = nn.Sequential(
            nn.Linear(128, 64), nn.ReLU(),
            nn.Linear(64, n_products * 3)  # add/remove/maintain per product
        )
        self.amount_head = nn.Sequential(
            nn.Linear(128, 64), nn.ReLU(),
            nn.Linear(64, n_products), nn.ReLU()
        )

    def forward(self, x):
        out, (hn, cn) = self.lstm(x)
        last = hn[-1]  # final hidden state
        scores = self.product_head(last)
        adjustments = self.adjustment_head(last).view(-1, 4, 3)
        amounts = self.amount_head(last) * 2000  # scale to mg
        return scores, adjustments, amounts


class RoutineDataset(Dataset):
    """Synthetic routine + skin condition data."""
    def __init__(self, n_samples=400):
        np.random.seed(42)
        self.samples = []
        self.labels = []
        for _ in range(n_samples):
            # 90-day history: condition scores + product usage
            seq = np.random.normal(0.5, 0.15, (DAYS, FEATURES_PER_DAY)).astype(np.float32)
            # Simulate improvement: products applied → condition improves over time
            for d in range(1, DAYS):
                seq[d, 0] = max(0, seq[d-1, 0] - 0.005 * seq[d, 4])  # condition improves
            self.samples.append(seq)
            # Labels: product efficacy scores
            scores = np.random.uniform(0.3, 0.9, 4).astype(np.float32)
            self.labels.append(scores)

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return torch.tensor(self.samples[idx]), torch.tensor(self.labels[idx])


def train():
    dataset = RoutineDataset(n_samples=400)
    loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)

    model = RoutineOptimizer().to(DEVICE)
    optimizer = AdamW(model.parameters(), lr=LR)
    criterion = nn.MSELoss()

    for epoch in range(EPOCHS):
        model.train()
        total_loss = 0
        for x, y in loader:
            x, y = x.to(DEVICE), y.to(DEVICE)
            optimizer.zero_grad()
            scores, adj, amounts = model(x)
            loss = criterion(scores, y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{EPOCHS} — Loss: {total_loss/len(loader):.4f}")

    torch.save(model.state_dict(), MODEL_SAVE)
    print(f"✓ Routine optimizer saved to {MODEL_SAVE}")


if __name__ == "__main__":
    train()
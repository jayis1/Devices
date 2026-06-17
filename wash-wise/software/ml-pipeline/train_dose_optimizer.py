#!/usr/bin/env python3
"""
WashWise — Dose Optimization ML Training

Trains an XGBoost model to predict optimal detergent dose based on:
- Fabric type
- Stain type
- Estimated load weight
- Water hardness
- Detergent brand concentration

The model learns from your machine's actual response over time (transfer learning).
Initially uses a rule-based baseline; improves as more wash cycles complete.

Usage:
    python3 train_dose_optimizer.py --cycles 5000
"""

import argparse
import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
import pickle

# Fabric care: [max_temp, base_detergent_ml]
FABRIC_BASE = {
    0: (40, 30),   # unknown
    1: (60, 35),   # cotton
    2: (40, 25),   # polyester
    3: (30, 20),   # wool
    4: (30, 15),   # silk
    5: (40, 35),   # denim
    6: (30, 25),   # nylon
    7: (40, 30),   # linen
    8: (40, 30),   # blend
}

# Stain adjustments: extra detergent mL
STAIN_EXTRA = {
    0: 0,    # clean
    1: 10,   # coffee
    2: 15,   # wine
    3: 10,   # blood
    4: 20,   # grease
    5: 10,   # grass
    6: 10,   # ink
    7: 10,   # food
    8: 5,    # sweat
    9: 15,   # rust
    10: 10,  # unknown
}

# Water hardness scale: 0=soft, 100=very hard (mg/L CaCO3)
# Hard water needs more detergent


def generate_training_data(n_cycles=5000):
    """Generate synthetic wash cycle data with optimal dose labels."""
    np.random.seed(42)

    # Features: fabric_type, stain_type, load_kg, water_hardness,
    #           detergent_concentration (0-1), soil_level (0-3)
    X = np.zeros((n_cycles, 6))
    y = np.zeros(n_cycles)

    for i in range(n_cycles):
        fabric = np.random.randint(0, 9)
        stain = np.random.randint(0, 11)
        load_kg = np.random.uniform(1.0, 8.0)
        water_hardness = np.random.uniform(0, 250)  # mg/L CaCO3
        det_conc = np.random.uniform(0.5, 1.5)  # relative concentration
        soil_level = np.random.randint(0, 4)  # 0=clean, 3=heavy soil

        # Compute optimal dose (rule-based baseline + noise)
        base_temp, base_ml = FABRIC_BASE[fabric]
        stain_extra = STAIN_EXTRA[stain]

        # Scale by load weight (more clothes = more detergent, but sub-linear)
        load_factor = 0.5 + 0.5 * np.sqrt(load_kg / 4.0)

        # Hard water needs more detergent
        hard_factor = 1.0 + (water_hardness / 250.0) * 0.5

        # Concentration adjustment (concentrated detergent = less volume)
        conc_factor = 1.0 / det_conc

        # Soil level adds detergent
        soil_extra = soil_level * 5

        optimal_ml = (base_ml + stain_extra + soil_extra) * load_factor * hard_factor * conc_factor
        optimal_ml += np.random.normal(0, 2.0)  # measurement noise
        optimal_ml = np.clip(optimal_ml, 5, 120)

        X[i] = [fabric, stain, load_kg, water_hardness, det_conc, soil_level]
        y[i] = optimal_ml

    return X, y


def train(args):
    print("Generating wash cycle training data...")
    X, y = generate_training_data(args.cycles)
    print(f"Dataset: {X.shape[0]} cycles, {X.shape[1]} features")

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    # XGBoost regressor
    model = xgb.XGBRegressor(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        objective='reg:squarederror',
    )

    print("Training XGBoost dose optimizer...")
    model.fit(X_train, y_train,
              eval_set=[(X_test, y_test)],
              verbose=False)

    # Evaluate
    train_score = model.score(X_train, y_train)
    test_score = model.score(X_test, y_test)
    rmse = np.sqrt(np.mean((model.predict(X_test) - y_test) ** 2))
    print(f"\nR² train: {train_score:.4f}")
    print(f"R² test:  {test_score:.4f}")
    print(f"RMSE:     {rmse:.2f} mL")

    # Feature importance
    feat_names = ['fabric', 'stain', 'load_kg', 'water_hardness', 'det_conc', 'soil_level']
    importances = model.feature_importances_
    for name, imp in sorted(zip(feat_names, importances), key=lambda x: -x[1]):
        print(f"  {name:20s}: {imp:.3f}")

    # Save model
    with open(args.output, 'wb') as f:
        pickle.dump(model, f)
    print(f"\nModel saved to {args.output}")

    # Export as JSON for cloud deployment
    model.save_model(args.output.replace('.pkl', '.json'))
    print(f"XGBoost JSON saved to {args.output.replace('.pkl', '.json')}")


def main():
    parser = argparse.ArgumentParser(description="Train WashWise dose optimizer")
    parser.add_argument("--cycles", type=int, default=5000, help="Training cycles")
    parser.add_argument("--output", default="dose_optimizer.pkl", help="Output model")
    args = parser.parse_args()
    train(args)


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
MosquitoSync — ML Training Pipeline Runner

Trains all 6 models in the MosquitoSync ML pipeline:
  1. WingNet           — Mosquito species classification CNN
  2. ActivityForecast  — 72-hour activity LSTM
  3. DiseaseRisk       — Dengue/West Nile/Malaria XGBoost
  4. BiteRisk          — Personal bite risk XGBoost
  5. CaptureCount      — Trap capture U-Net-tiny
  6. SensorAnomaly     — Isolation Forest

Usage: python train_models.py [--model all|wingnet|activity|disease|bite|capture|anomaly]
"""
from __future__ import annotations

import argparse
import sys
import os

# Add ml-pipeline to path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "software", "ml-pipeline")
sys.path.insert(0, ML_DIR)


def train_wingnet() -> None:
    print("\n" + "=" * 60)
    print("  Training WingNet (Mosquito Species CNN)")
    print("=" * 60)
    from train_wingnet import train_wingnet
    train_wingnet(data_dir="data/wingbeats", epochs=50)


def train_activity() -> None:
    print("\n" + "=" * 60)
    print("  Training ActivityForecast (72h Activity LSTM)")
    print("=" * 60)
    from train_activity_forecast import train_activity_forecast
    train_activity_forecast(epochs=50)


def train_disease() -> None:
    print("\n" + "=" * 60)
    print("  Training DiseaseRisk (Dengue/West Nile/Malaria XGBoost)")
    print("=" * 60)
    from train_disease_risk import train_all_disease_models
    train_all_disease_models()


def train_bite() -> None:
    print("\n" + "=" * 60)
    print("  Training BiteRisk (Personal Bite Risk XGBoost)")
    print("=" * 60)
    from train_bite_risk import train_bite_risk
    train_bite_risk()


def train_capture() -> None:
    print("\n" + "=" * 60)
    print("  Training CaptureCount (Trap Capture U-Net-tiny)")
    print("=" * 60)
    from train_capture_count import train_capture_count
    train_capture_count(epochs=50)


def train_anomaly() -> None:
    print("\n" + "=" * 60)
    print("  Training SensorAnomaly (Isolation Forest)")
    print("=" * 60)
    from train_sensor_anomaly import train_sensor_anomaly
    train_sensor_anomaly()


TRAINERS = {
    "wingnet": train_wingnet,
    "activity": train_activity,
    "disease": train_disease,
    "bite": train_bite,
    "capture": train_capture,
    "anomaly": train_anomaly,
}


def main() -> None:
    parser = argparse.ArgumentParser(
        description="MosquitoSync ML training pipeline runner"
    )
    parser.add_argument(
        "--model",
        default="all",
        choices=["all"] + list(TRAINERS.keys()),
        help="Which model to train (default: all)",
    )
    args = parser.parse_args()

    # Create models directory
    os.makedirs("models", exist_ok=True)

    if args.model == "all":
        for name, trainer in TRAINERS.items():
            try:
                trainer()
            except Exception as e:
                print(f"  ERROR training {name}: {e}")
                continue
    else:
        TRAINERS[args.model]()

    print("\n" + "=" * 60)
    print("  Training complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
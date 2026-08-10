#!/usr/bin/env python3
"""
FireSync — ML Training Pipeline Runner

Trains all 6 models in the FireSync ML pipeline:
  1. FlameNet        — Multi-modal fire classification CNN
  2. ThermalAnomaly — LSTM autoencoder for thermal anomaly detection
  3. ArcDetect      — Electrical arc fault detection CNN
  4. EscapeRouter   — Dijkstra escape route optimization
  5. OccupantTracker — HMM multi-room occupant tracking
  6. RiskForecast    — XGBoost 7-day fire risk forecast
  7. SensorAnomaly  — Isolation Forest sensor fault detection

Usage: python train_models.py [--model all|flamenet|thermal|arcdetect|escape|occupant|risk|anomaly]
"""
from __future__ import annotations

import argparse
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "software", "ml-pipeline")
sys.path.insert(0, ML_DIR)


def train_flamenet() -> None:
    print("\n" + "=" * 60)
    print("  Training FlameNet (Multi-Modal Fire Classification CNN)")
    print("=" * 60)
    from train_flamenet import train_flamenet
    train_flamenet(data_dir="data/fire", epochs=50)


def train_thermal() -> None:
    print("\n" + "=" * 60)
    print("  Training ThermalAnomaly (LSTM Autoencoder)")
    print("=" * 60)
    from train_thermal_anomaly import train_thermal_anomaly
    train_thermal_anomaly(epochs=50)


def train_arcdetect() -> None:
    print("\n" + "=" * 60)
    print("  Training ArcDetect (Electrical Arc Fault CNN)")
    print("=" * 60)
    from train_arcdetect import train_arcdetect
    train_arcdetect(epochs=50)


def train_escape() -> None:
    print("\n" + "=" * 60)
    print("  Training EscapeRouter (Dijkstra + Learned Weights)")
    print("=" * 60)
    from train_escape_router import train_escape_router
    train_escape_router(data_dir="data/escape", epochs=50)


def train_occupant() -> None:
    print("\n" + "=" * 60)
    print("  Training OccupantTracker (Hidden Markov Model)")
    print("=" * 60)
    from train_occupant_tracker import train_occupant_tracker
    train_occupant_tracker()


def train_risk() -> None:
    print("\n" + "=" * 60)
    print("  Training RiskForecast (XGBoost 7-Day Fire Risk)")
    print("=" * 60)
    from train_risk_forecast import train_risk_forecast
    train_risk_forecast()


def train_anomaly() -> None:
    print("\n" + "=" * 60)
    print("  Training SensorAnomaly (Isolation Forest)")
    print("=" * 60)
    from train_sensor_anomaly import train_sensor_anomaly
    train_sensor_anomaly()


TRAINERS = {
    "flamenet": train_flamenet,
    "thermal": train_thermal,
    "arcdetect": train_arcdetect,
    "escape": train_escape,
    "occupant": train_occupant,
    "risk": train_risk,
    "anomaly": train_anomaly,
}


def main() -> None:
    parser = argparse.ArgumentParser(
        description="FireSync ML training pipeline runner"
    )
    parser.add_argument(
        "--model",
        default="all",
        choices=["all"] + list(TRAINERS.keys()),
        help="Which model to train (default: all)",
    )
    args = parser.parse_args()

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
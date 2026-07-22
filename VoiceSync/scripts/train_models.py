#!/usr/bin/env python3
"""
VoiceSync — ML Training Pipeline Runner

Trains all 6 models in the VoiceSync ML pipeline:
  1. VoiceNet       — Voice quality classification CNN
  2. VocalLoad      — Cumulative vocal dose XGBoost
  3. VoiceRisk      — 7-day disorder risk LSTM
  4. RefluxDetect   — Laryngopharyngeal reflux 1D-CNN
  5. HydrationModel — Hydration status XGBoost
  6. VocalAnomaly   — Vocal anomaly Isolation Forest

Usage: python train_models.py [--model all|voicenet|load|risk|reflux|hydration|anomaly]
"""
from __future__ import annotations

import argparse
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "software", "ml-pipeline")
sys.path.insert(0, ML_DIR)


def train_voicenet() -> None:
    print("\n" + "=" * 60)
    print("  Training VoiceNet (Voice Quality CNN)")
    print("=" * 60)
    from train_voicenet import train_voicenet
    train_voicenet(epochs=50)


def train_load() -> None:
    print("\n" + "=" * 60)
    print("  Training VocalLoad (Cumulative Vocal Dose XGBoost)")
    print("=" * 60)
    from train_vocal_load import train_vocal_load
    train_vocal_load()


def train_risk() -> None:
    print("\n" + "=" * 60)
    print("  Training VoiceRisk (7-Day Disorder Risk LSTM)")
    print("=" * 60)
    from train_voice_risk import train_voice_risk
    train_voice_risk(epochs=50)


def train_reflux() -> None:
    print("\n" + "=" * 60)
    print("  Training RefluxDetect (LPR 1D-CNN)")
    print("=" * 60)
    from train_reflux_detect import train_reflux_detect
    train_reflux_detect(epochs=50)


def train_hydration() -> None:
    print("\n" + "=" * 60)
    print("  Training HydrationModel (Hydration Status XGBoost)")
    print("=" * 60)
    from train_hydration_model import train_hydration_model
    train_hydration_model()


def train_anomaly() -> None:
    print("\n" + "=" * 60)
    print("  Training VocalAnomaly (Vocal Anomaly Isolation Forest)")
    print("=" * 60)
    from train_vocal_anomaly import train_vocal_anomaly
    train_vocal_anomaly()


TRAINERS = {
    "voicenet": train_voicenet,
    "load": train_load,
    "risk": train_risk,
    "reflux": train_reflux,
    "hydration": train_hydration,
    "anomaly": train_anomaly,
}


def main() -> None:
    parser = argparse.ArgumentParser(
        description="VoiceSync ML training pipeline runner"
    )
    parser.add_argument(
        "--model", default="all",
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
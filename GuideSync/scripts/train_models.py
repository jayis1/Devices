#!/usr/bin/env python3
"""
GuideSync — ML Training Pipeline Runner

Trains all 6 models in the GuideSync ML pipeline:
  1. SceneNet       — YOLOv8-nano object detection
  2. ObstacleNet    — ToF depth grid hazard CNN
  3. TextReader     — EAST + CRNN OCR
  4. NavNet         — BLE beacon indoor positioning LSTM
  5. CrosswalkNet   — Crosswalk/signal MobileNetV3
  6. FallNet        — Fall detection 1D-CNN
  7. SensorAnomaly  — Isolation Forest

Usage: python train_models.py [--model all|scenenet|obstaclenet|textreader|navnet|crosswalknet|fallnet|anomaly]
"""
from __future__ import annotations

import argparse
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "software", "ml-pipeline")
sys.path.insert(0, ML_DIR)


def train_scenenet() -> None:
    print("\n" + "=" * 60)
    print("  Training SceneNet (YOLOv8-nano Object Detection)")
    print("=" * 60)
    from train_scenenet import train_scenenet
    train_scenenet(data_dir="data/coco", epochs=50)


def train_obstaclenet() -> None:
    print("\n" + "=" * 60)
    print("  Training ObstacleNet (ToF Depth Grid CNN)")
    print("=" * 60)
    from train_obstaclenet import train_obstaclenet
    train_obstaclenet(epochs=50)


def train_textreader() -> None:
    print("\n" + "=" * 60)
    print("  Training TextReader (EAST + CRNN OCR)")
    print("=" * 60)
    from train_textreader import train_textreader
    train_textreader(data_dir="data/text", epochs=50)


def train_navnet() -> None:
    print("\n" + "=" * 60)
    print("  Training NavNet (BLE Beacon Indoor Positioning LSTM)")
    print("=" * 60)
    from train_navnet import train_navnet
    train_navnet(epochs=50)


def train_crosswalknet() -> None:
    print("\n" + "=" * 60)
    print("  Training CrosswalkNet (Crosswalk & Signal MobileNetV3)")
    print("=" * 60)
    from train_crosswalknet import train_crosswalknet
    train_crosswalknet(epochs=50)


def train_fallnet() -> None:
    print("\n" + "=" * 60)
    print("  Training FallNet (Fall Detection 1D-CNN)")
    print("=" * 60)
    from train_fallnet import train_fallnet
    train_fallnet(epochs=50)


def train_anomaly() -> None:
    print("\n" + "=" * 60)
    print("  Training SensorAnomaly (Isolation Forest)")
    print("=" * 60)
    from train_sensor_anomaly import train_sensor_anomaly
    train_sensor_anomaly()


TRAINERS = {
    "scenenet": train_scenenet,
    "obstaclenet": train_obstaclenet,
    "textreader": train_textreader,
    "navnet": train_navnet,
    "crosswalknet": train_crosswalknet,
    "fallnet": train_fallnet,
    "anomaly": train_anomaly,
}


def main() -> None:
    parser = argparse.ArgumentParser(
        description="GuideSync ML training pipeline runner"
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
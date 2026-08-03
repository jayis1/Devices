#!/usr/bin/env python3
"""
MenoSync — Master ML Training Script

Trains all 6 models in the MenoSync ML pipeline.

Usage:
  python train_models.py --all --data /data/menosync
  python train_models.py --model hotflash --data /data/menosync/hotflash
"""
import argparse
import subprocess
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(SCRIPT_DIR, "..", "software", "ml-pipeline")

MODELS = {
    "hotflash": {
        "script": "train_hotflash.py",
        "default_epochs": 80,
    },
    "nightsweat": {
        "script": "train_nightsweat.py",
        "default_epochs": 100,
    },
    "sleepquality": {
        "script": "train_sleepquality.py",
        "default_epochs": 120,
    },
    "mood": {
        "script": "train_mood.py",
        "default_epochs": 100,
    },
    "bonerisk": {
        "script": "train_bonerisk.py",
        "default_epochs": 300,
    },
    "cooling": {
        "script": "train_cooling.py",
        "default_epochs": 10000,
    },
}


def train_model(model_name, data_path, epochs=None, output="models"):
    config = MODELS[model_name]
    script_path = os.path.join(ML_DIR, config["script"])
    ep = epochs or config["default_epochs"]

    print(f"\n{'='*60}")
    print(f"Training: {model_name} ({config['script']})")
    print(f"Data: {data_path}")
    print(f"Epochs: {ep}")
    print(f"Output: {output}")
    print(f"{'='*60}\n")

    cmd = [
        sys.executable, script_path,
        "--data", data_path,
        "--epochs", str(ep),
        "--output", output,
    ]
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"✗ Training failed for {model_name}")
        return False
    print(f"✓ Training complete for {model_name}")
    return True


def main():
    parser = argparse.ArgumentParser(description="MenoSync master ML training")
    parser.add_argument("--all", action="store_true", help="Train all models")
    parser.add_argument("--model", choices=list(MODELS.keys()),
                        help="Specific model to train")
    parser.add_argument("--data", type=str, required=True,
                        help="Path to data directory (or base for --all)")
    parser.add_argument("--epochs", type=int, default=None,
                        help="Override default epochs")
    parser.add_argument("--output", type=str, default="models",
                        help="Output directory for trained models")
    args = parser.parse_args()

    if not args.all and not args.model:
        parser.error("Specify --all or --model")

    os.makedirs(args.output, exist_ok=True)
    results = {}

    if args.all:
        for name in MODELS:
            data_path = os.path.join(args.data, name)
            if not os.path.exists(data_path):
                print(f"⚠️  Data not found for {name}: {data_path}, skipping")
                results[name] = "skipped"
                continue
            success = train_model(name, data_path, args.epochs, args.output)
            results[name] = "success" if success else "failed"
    else:
        success = train_model(args.model, args.data, args.epochs, args.output)
        results[args.model] = "success" if success else "failed"

    print(f"\n{'='*60}")
    print("Training Summary:")
    print(f"{'='*60}")
    for name, status in results.items():
        icon = "✓" if status == "success" else "✗" if status == "failed" else "⊘"
        print(f"  {icon} {name}: {status}")


if __name__ == "__main__":
    main()
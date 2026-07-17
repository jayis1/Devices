#!/usr/bin/env python3
"""
StormSync — ML Training Pipeline Runner

Trains all 6 models in sequence and reports results.

Usage:
  python train_models.py
  python train_models.py --model floodforecast
"""

import argparse
import sys
import time
from pathlib import Path

ML_DIR = Path(__file__).parent.parent / "software" / "ml-pipeline"


def run_training(model_name: str):
    """Run a single model training script."""
    script_map = {
        "floodforecast": "train_floodforecast.py",
        "pumphealth": "train_pumphealth.py",
        "soilsat": "train_soilsat.py",
        "rainfall_runoff": "train_rainfall_runoff.py",
        "storm_risk": "train_storm_risk.py",
        "sensor_anomaly": "train_sensor_anomaly.py",
    }

    if model_name not in script_map:
        print(f"Unknown model: {model_name}")
        print(f"Available: {', '.join(script_map.keys())}")
        return False

    script = ML_DIR / script_map[model_name]
    if not script.exists():
        print(f"Training script not found: {script}")
        return False

    print(f"\n{'='*60}")
    print(f"  Training: {model_name}")
    print(f"  Script: {script_map[model_name]}")
    print(f"{'='*60}\n")

    start = time.time()

    # Import and run the training module
    import importlib.util
    spec = importlib.util.spec_from_file_location(model_name, script)
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
        # Most scripts have train_model() or train_and_save()
        if hasattr(module, 'train_model'):
            result = module.train_model()
        elif hasattr(module, 'train_and_save'):
            result = module.train_and_save()
        else:
            print(f"No train function found in {script_map[model_name]}")
            return False
    except Exception as e:
        print(f"ERROR training {model_name}: {e}")
        return False

    elapsed = time.time() - start
    print(f"\n✓ {model_name} trained in {elapsed:.1f}s (result: {result})")
    return True


def main():
    parser = argparse.ArgumentParser(description="StormSync ML training pipeline runner")
    parser.add_argument("--model", type=str, default=None,
                        help="Train specific model (default: all)")
    args = parser.parse_args()

    all_models = ["floodforecast", "pumphealth", "soilsat",
                  "rainfall_runoff", "storm_risk", "sensor_anomaly"]

    if args.model:
        models = [args.model]
    else:
        models = all_models

    print(f"\nStormSync ML Training Pipeline")
    print(f"Models to train: {', '.join(models)}")
    print(f"ML directory: {ML_DIR}")

    results = {}
    for model in models:
        success = run_training(model)
        results[model] = "✓ PASS" if success else "✗ FAIL"

    print(f"\n{'='*60}")
    print(f"  Training Summary")
    print(f"{'='*60}")
    for model, status in results.items():
        print(f"  {model:20s} {status}")
    print(f"{'='*60}")

    if all(v == "✓ PASS" for v in results.values()):
        print("\n✓ All models trained successfully!")
    else:
        print("\n⚠ Some models failed — check output above")
        sys.exit(1)


if __name__ == "__main__":
    main()
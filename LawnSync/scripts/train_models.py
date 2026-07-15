#!/usr/bin/env python3
"""
LawnSync — Master ML Training Script

Runs the full 6-model ML pipeline:
1. DiseaseNet (15-class disease classification)
2. WeedSeg (9-class weed segmentation)
3. IrrigationRL (DQN irrigation scheduler)
4. SoilForecast (14-day LSTM moisture forecast)
5. DroughtNet (4-class drought stress)
6. FertScheduler (XGBoost fertilization timing)

Usage:
    python train_models.py              # Train all
    python train_models.py --model disease   # Train specific model
"""

import argparse
import subprocess
import sys
import os
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ML_DIR = os.path.join(SCRIPT_DIR, "..", "software", "ml-pipeline")

MODELS = {
    "disease": {
        "name": "DiseaseNet (15-class disease classification)",
        "script": "train_diseasenet.py",
        "description": "MobileNetV3-Small + custom head, 15 lawn disease classes",
    },
    "weed": {
        "name": "WeedSeg (9-class weed segmentation)",
        "script": "train_weedseg.py",
        "description": "U-Net-tiny with MobileNetV2 encoder, semantic segmentation",
    },
    "irrigation": {
        "name": "IrrigationRL (DQN irrigation scheduler)",
        "script": "train_irrigation_rl.py",
        "description": "Deep Q-Network for optimal irrigation scheduling",
    },
    "soil": {
        "name": "SoilForecast (14-day LSTM)",
        "script": "train_soil_forecast.py",
        "description": "2-layer LSTM for 14-day soil moisture prediction",
    },
    "drought_fert": {
        "name": "DroughtNet + FertScheduler",
        "script": "train_drought_fert.py",
        "description": "1D-CNN drought stress + XGBoost fertilization timing",
    },
}


def run_model(key: str) -> bool:
    """Train a single model. Returns True on success."""
    model = MODELS[key]
    print(f"\n{'='*60}")
    print(f"  Training: {model['name']}")
    print(f"  Script:   {model['script']}")
    print(f"  Desc:     {model['description']}")
    print(f"{'='*60}\n")

    script_path = os.path.join(ML_DIR, model["script"])
    start = time.time()
    result = subprocess.run([sys.executable, script_path], cwd=ML_DIR)
    elapsed = time.time() - start

    if result.returncode == 0:
        print(f"\n✓ {model['name']} — completed in {elapsed:.0f}s")
        return True
    else:
        print(f"\n✗ {model['name']} — FAILED (exit {result.returncode})")
        return False


def main():
    parser = argparse.ArgumentParser(description="LawnSync ML training pipeline")
    parser.add_argument("--model", choices=list(MODELS.keys()) + ["all"],
                        default="all", help="Model to train (default: all)")
    args = parser.parse_args()

    print("╔══════════════════════════════════════════════════════╗")
    print("║     LawnSync ML Pipeline — Master Training Script    ║")
    print("║     6 models: Disease, Weed, Irrigation,             ║")
    print("║                Soil Forecast, Drought, Fertilizer   ║")
    print("╚══════════════════════════════════════════════════════╝")

    if args.model == "all":
        results = {}
        for key in MODELS:
            results[key] = run_model(key)

        print(f"\n{'='*60}")
        print("  Training Summary")
        print(f"{'='*60}")
        for key, success in results.items():
            status = "✓ PASS" if success else "✗ FAIL"
            print(f"  {MODELS[key]['name']}: {status}")

        total = len(results)
        passed = sum(results.values())
        print(f"\n  {passed}/{total} models trained successfully")
        if passed < total:
            sys.exit(1)
    else:
        success = run_model(args.model)
        sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
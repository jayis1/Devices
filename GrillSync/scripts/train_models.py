#!/usr/bin/env python3
"""
GrillSync — Model Training Orchestrator

Trains all 6 models in the GrillSync ML pipeline.
"""
import subprocess
import sys
import os


MODELS = [
    ("DonenessNet", "train_doneness.py", "models/doneness_v2.pth"),
    ("FlareUpNet", "train_flareup.py", "models/flareup_v1.pth"),
    ("GasLeakNet", "train_gasleak.py", "models/gasleak_v1.json"),
    ("SmokeNet", "train_smoke.py", "models/smoke_v1.pth"),
    ("GrillAnomaly", "train_anomaly.py", "models/grill_anomaly.pkl"),
    ("SafetyForecast", "train_safety.py", "models/safety_forecast_v1.pth"),
]


def main():
    print("GrillSync ML Pipeline — Training All Models")
    print("=" * 50)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    ml_dir = os.path.join(script_dir, "..", "software", "ml-pipeline")
    os.makedirs(os.path.join(ml_dir, "models"), exist_ok=True)

    for name, script, output in MODELS:
        print(f"\n{'='*40}")
        print(f"Training {name}...")
        print(f"  Script: {script}")
        print(f"  Output: {output}")
        print(f"{'='*40}")

        result = subprocess.run(
            [sys.executable, os.path.join(ml_dir, script),
             "--output", os.path.join(ml_dir, output)],
            cwd=ml_dir,
        )

        if result.returncode != 0:
            print(f"✗ {name} training FAILED (exit code {result.returncode})")
        else:
            print(f"✓ {name} training complete")

    print("\n" + "=" * 50)
    print("All model training complete!")
    print("=" * 50)

    # List model files
    models_dir = os.path.join(ml_dir, "models")
    if os.path.exists(models_dir):
        print("\nGenerated model files:")
        for f in sorted(os.listdir(models_dir)):
            size = os.path.getsize(os.path.join(models_dir, f))
            print(f"  {f}: {size:,} bytes")


if __name__ == "__main__":
    main()
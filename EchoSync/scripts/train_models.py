#!/usr/bin/env python3
"""
EchoSync — Train All Models Script

Runs the full 6-model ML pipeline for the EchoSync Sound Awareness System.
"""
import subprocess
import sys
import os
import argparse


MODELS = [
    ("SoundNet", "train_soundnet.py", ["--epochs", "100", "--batch-size", "64"]),
    ("AlertPriority", "train_alert_priority.py", ["--n-samples", "10000"]),
    ("SoundLocalize", "train_sound_localize.py", ["--epochs", "50", "--n-samples", "5000"]),
    ("SoundAnomaly", "train_sound_anomaly.py", ["--n-samples", "5000"]),
    ("PersonalSound", "train_personal_sound.py", ["--epochs", "50"]),
    ("DailySoundLog", "train_daily_sound_log.py", ["--epochs", "30"]),
]


def main():
    parser = argparse.ArgumentParser(description="Train all EchoSync ML models")
    parser.add_argument("--models-dir", default="./models", help="Output directory")
    parser.add_argument("--skip", nargs="*", default=[], help="Models to skip")
    args = parser.parse_args()

    os.makedirs(args.models_dir, exist_ok=True)
    script_dir = os.path.dirname(os.path.abspath(__file__))

    for name, script, extra_args in MODELS:
        if name in args.skip:
            print(f"\n=== Skipping {name} ===")
            continue

        print(f"\n{'='*60}")
        print(f"=== Training {name} ===")
        print(f"{'='*60}")

        cmd = [sys.executable, script] + extra_args + ["--output", args.models_dir]
        result = subprocess.run(cmd, cwd=script_dir)

        if result.returncode != 0:
            print(f"ERROR: {name} training failed (exit code {result.returncode})")
            sys.exit(1)

        print(f"✓ {name} training complete")

    print(f"\n{'='*60}")
    print(f"=== All models trained successfully ===")
    print(f"Models saved to: {args.models_dir}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
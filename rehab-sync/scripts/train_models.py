#!/usr/bin/env python3
"""
RehabSync — Model Training Orchestrator

Trains all 6 ML models for the RehabSync system.
Can train individually or all at once.

Usage:
  python train_models.py --all
  python train_models.py --model exercise
  python train_models.py --model exercise,form,recovery
"""
import argparse
import subprocess
import sys
import os

MODELS = {
    "exercise": {
        "script": "train_exercise.py",
        "args": ["--epochs", "100", "--export-tflite"],
        "description": "ExerciseNet (1D-CNN, 30-class exercise recognition)",
    },
    "form": {
        "script": "train_form.py",
        "args": ["--epochs", "200"],
        "description": "FormNet (Temporal CNN, form score + deviation)",
    },
    "rep_count": {
        "script": "train_rep_count.py",
        "args": [],
        "description": "RepCount (Peak detection + state machine)",
    },
    "recovery": {
        "script": "train_recovery.py",
        "args": ["--epochs", "500"],
        "description": "RecoveryLSTM (2-layer LSTM, 8-week milestone forecast)",
    },
    "adherence": {
        "script": "train_adherence.py",
        "args": [],
        "description": "AdherenceRF (Random Forest, 7-day dropout prediction)",
    },
    "anomaly": {
        "script": "train_anomaly.py",
        "args": [],
        "description": "AnomalyIF (Isolation Forest, compensation pattern detection)",
    },
}


def train_model(model_name, data_dir, output_dir):
    if model_name not in MODELS:
        print(f"Unknown model: {model_name}")
        return False

    model = MODELS[model_name]
    script_path = os.path.join(os.path.dirname(__file__), "..", "software", "ml-pipeline", model["script"])

    cmd = [sys.executable, script_path,
           "--data", os.path.join(data_dir, model_name),
           "--output", output_dir] + model["args"]

    print(f"\n{'='*60}")
    print(f"Training: {model_name} — {model['description']}")
    print(f"Command: {' '.join(cmd)}")
    print(f"{'='*60}\n")

    result = subprocess.run(cmd, cwd=os.path.dirname(script_path))
    return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(description="Train RehabSync ML models")
    parser.add_argument("--all", action="store_true", help="Train all models")
    parser.add_argument("--model", type=str, help="Comma-separated model names to train")
    parser.add_argument("--data", type=str, default="/data/rehab-sync",
                       help="Root data directory (subdirs per model)")
    parser.add_argument("--output", type=str, default="models",
                       help="Output directory for trained models")
    args = parser.parse_args()

    if args.all:
        models_to_train = list(MODELS.keys())
    elif args.model:
        models_to_train = [m.strip() for m in args.model.split(",")]
    else:
        parser.print_help()
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)

    results = {}
    for model_name in models_to_train:
        success = train_model(model_name, args.data, args.output)
        results[model_name] = "✓ PASS" if success else "✗ FAIL"

    print(f"\n{'='*60}")
    print("Training Summary:")
    print(f"{'='*60}")
    for model, status in results.items():
        print(f"  {model:15s} {status}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
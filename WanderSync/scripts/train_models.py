#!/usr/bin/env python3
"""
WanderSync — ML Training Pipeline Runner

Runs all 7 model training scripts in sequence and reports results.
"""
import subprocess
import sys
import time
import os


MODELS = [
    ("WanderNet", "train_wandernet.py", "Wandering risk prediction LSTM"),
    ("ADLNet", "train_adlnet.py", "Activity of daily living CNN"),
    ("CogDecline", "train_cogdecline.py", "Cognitive decline XGBoost"),
    ("AnomalyDetect", "train_anomalydetect.py", "Behavioral anomaly Isolation Forest"),
    ("RoutePredict", "train_routepredict.py", "Wandering route prediction LSTM"),
    ("ReminderOpt", "train_reminderopt.py", "Reminder timing DQN"),
    ("SleepNet", "train_sleepnet.py", "Sleep quality BiLSTM"),
]


def main():
    print("=" * 60)
    print("WanderSync — ML Training Pipeline (7 models)")
    print("=" * 60)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    ml_dir = os.path.join(script_dir, "..", "software", "ml-pipeline")

    results = []
    for name, script, desc in MODELS:
        print(f"\n{'─' * 60}")
        print(f"Training {name}: {desc}")
        print(f"{'─' * 60}")

        start = time.time()
        script_path = os.path.join(ml_dir, script)

        try:
            result = subprocess.run(
                [sys.executable, script_path],
                capture_output=True, text=True, timeout=600
            )
            elapsed = time.time() - start
            success = result.returncode == 0

            if success:
                print(f"  ✅ {name} trained in {elapsed:.1f}s")
                # Print last 3 lines of output
                lines = result.stdout.strip().split('\n')
                for line in lines[-3:]:
                    print(f"     {line}")
            else:
                print(f"  ❌ {name} failed (exit {result.returncode})")
                print(f"     {result.stderr[:200]}")

            results.append((name, success, elapsed))

        except subprocess.TimeoutExpired:
            print(f"  ⏰ {name} timed out after 600s")
            results.append((name, False, 600))
        except Exception as e:
            print(f"  ❌ {name} error: {e}")
            results.append((name, False, 0))

    # Summary
    print(f"\n{'=' * 60}")
    print("Training Summary")
    print(f"{'=' * 60}")
    for name, success, elapsed in results:
        status = "✅" if success else "❌"
        print(f"  {status} {name:20s} {elapsed:7.1f}s")

    total_success = sum(1 for _, s, _ in results if s)
    print(f"\n  {total_success}/{len(results)} models trained successfully")
    print(f"  Total time: {sum(t for _, _, t in results):.1f}s")


if __name__ == "__main__":
    main()
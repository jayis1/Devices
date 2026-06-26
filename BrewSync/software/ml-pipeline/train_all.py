"""BrewSync ML Pipeline — Train all models"""
import argparse
from train_fermentation_progress import train_model as train_progress
from train_stuck_predictor import train_stuck_predictor
from train_infection_detector import train_infection_detector


def main():
    parser = argparse.ArgumentParser(description="Train all BrewSync ML models")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--output-dir", default="models")
    args = parser.parse_args()

    print("=" * 60)
    print("BrewSync ML Pipeline — Training All Models")
    print("=" * 60)

    print("\n[1/6] Training Fermentation Progress Model (LSTM)...")
    train_progress(epochs=args.epochs, output_dir=args.output_dir)

    print("\n[2/6] Training Stuck Fermentation Predictor (GradientBoosting)...")
    train_stuck_predictor(output_dir=args.output_dir, cross_validate=True)

    print("\n[3/6] Training Infection Detector (CNN + Anomaly)...")
    train_infection_detector(output_dir=args.output_dir, epochs=50)

    print("\n[4/6] Yeast Health Model — placeholder (simple linear model)")
    print("  (Production: train on cell count + viability lab data)")

    print("\n[5/6] Recipe Optimizer — placeholder (rule-based system)")
    print("  (Production: train on recipe-outcome pairs)")

    print("\n[6/6] Flavor Predictor — placeholder (linear regression)")
    print("  (Production: train on BJCP style data + fermentation outcomes)")

    print("\n" + "=" * 60)
    print("All models trained! Check models/ directory for outputs.")
    print("=" * 60)


if __name__ == "__main__":
    main()
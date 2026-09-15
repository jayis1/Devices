# Authored by jayis1. Synthetic smoke baseline; not evidence of safety or prediction performance.
import argparse
import csv
import json
import random
from pathlib import Path

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="")
    parser.add_argument("--output", default="model-card.json")
    parser.add_argument("--seed", type=int, default=17)
    args = parser.parse_args()
    random.seed(args.seed)
    samples = []
    if args.input:
        with open(args.input, newline="", encoding="utf-8") as handle:
            samples = list(csv.DictReader(handle))
    if not samples:
        samples = [{"vibration_rms": str(1 + random.random()), "label": "normal"} for _ in range(24)]
    values = [float(row["vibration_rms"]) for row in samples]
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    Path(args.output).write_text(json.dumps({"author": "jayis1", "synthetic_or_local_data": not bool(args.input), "samples": len(values), "baseline_mean": mean, "baseline_std": variance ** 0.5, "limitation": "Smoke artifact only; physical performance is unvalidated."}, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()

"""Synthetic pipeline smoke demo: never use these scores as field evidence."""
import random

def main() -> None:
    random.seed(7)
    # Demonstration data only: temperature C, wetness proxy, maintenance label.
    samples = [(random.uniform(5, 55), random.random(), 1) for _ in range(20)]
    samples += [(random.uniform(10, 35), random.random() * 0.3, 0) for _ in range(20)]
    random.shuffle(samples)
    split = int(len(samples) * .8)
    train, test = samples[:split], samples[split:]
    # Transparent deterministic baseline rather than an unvalidated ML model.
    predictions = [int(temp > 40 or wet > .8) for temp, wet, _ in test]
    truth = [label for _, _, label in test]
    accuracy = sum(p == y for p, y in zip(predictions, truth)) / max(1, len(test))
    print(f"synthetic-only holdout n={len(test)} baseline_accuracy={accuracy:.3f}; no real-world claim")

if __name__ == "__main__": main()

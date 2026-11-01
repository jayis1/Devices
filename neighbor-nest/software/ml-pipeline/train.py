"""Synthetic-only moisture trend demonstration; not a plant-care model."""
import random

def main():
    rng = random.Random(7)
    samples = [(rng.uniform(0, 100), rng.choice((0, 1))) for _ in range(200)]
    cut = 150
    train, test = samples[:cut], samples[cut:]
    # Transparent baseline rule, no fitted model and no efficacy claim.
    threshold = sum(x for x, y in train if y) / max(1, sum(y for x, y in train))
    correct = sum((x < threshold) == bool(y) for x, y in test)
    print(f"synthetic_rows={len(samples)} train={len(train)} test={len(test)}")
    print(f"demo_threshold={threshold:.1f} test_accuracy={correct/len(test):.3f}")
    print("Synthetic smoke test only; use caretaker-defined thresholds in deployment.")

if __name__ == "__main__":
    main()

"""Synthetic-only prototype: generate toy calibration pairs; no field data or performance claims."""
import random

def main():
    rng = random.Random(7)
    pairs = [(rng.uniform(0, 100), min(100, max(0, rng.uniform(0, 100)))) for _ in range(100)]
    baseline_mae = sum(abs(x-y) for x, y in pairs) / len(pairs)
    print(f"synthetic_rows={len(pairs)} baseline_mae={baseline_mae:.2f}")

if __name__ == "__main__":
    main()

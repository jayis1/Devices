import argparse
import random

def run(seed: int = 7, samples: int = 1000) -> tuple[int, int]:
    rng = random.Random(seed)
    errors = 0
    for _ in range(samples):
        occupied = rng.random() < 0.55
        load = rng.uniform(0.7, 1.0) if occupied else rng.uniform(0.0, 0.3)
        prediction = load > 0.5
        errors += prediction != occupied
    print(f"synthetic samples={samples} threshold_errors={errors}; not measured performance")
    return samples, errors

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--synthetic', action='store_true')
    parser.add_argument('--samples', type=int, default=1000)
    args = parser.parse_args()
    if args.samples < 1:
        parser.error('--samples must be positive')
    run(samples=args.samples)

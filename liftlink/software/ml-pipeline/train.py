"""Synthetic-only baseline demonstration; no real lift outage dataset or claims."""
import argparse
import random

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--samples', type=int, default=100)
    args = parser.parse_args()
    if args.samples < 2:
        parser.error('--samples must be >= 2')
    random.seed(7)
    # Demonstration label is generated from the same simple feature rule.
    rows = [(x := random.random(), int(x < 0.25)) for _ in range(args.samples)]
    correct = sum(int((score < 0.25) == label) for score, label in rows)
    print(f'synthetic samples={len(rows)} baseline_correct={correct}; not real-world performance')

if __name__ == '__main__':
    main()

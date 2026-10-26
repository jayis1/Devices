import argparse
import json
import random

# Synthetic pipeline smoke demonstration only. Never represents field performance.
def make_data(n=200, seed=17):
    rng = random.Random(seed)
    rows = []
    for _ in range(n):
        door = rng.randrange(2)
        stale = rng.randrange(2)
        excursion = rng.randrange(2)
        label = int(door or stale or excursion)
        rows.append(([door, stale, excursion], label))
    return rows

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--epochs', type=int, default=1)
    args = parser.parse_args()
    if args.epochs < 1: parser.error('--epochs must be positive')
    rows = make_data()
    # Transparent rule baseline, no trained model or accuracy claim.
    correct = sum(int(any(features)) == label for features, label in rows)
    print(json.dumps({'rows': len(rows), 'rule_baseline_correct': correct,
                      'metrics_are_synthetic_only': True, 'epochs_requested': args.epochs}))

if __name__ == '__main__': main()

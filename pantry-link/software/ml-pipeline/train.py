import argparse
import csv
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FEATURES = ("weekday", "deliveries", "count")

def make_data(path: Path, seed: int = 7) -> None:
    rng = random.Random(seed)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["day", *FEATURES, "stockout_next_day"])
        for day in range(180):
            weekday = day % 7
            deliveries = int(weekday in (1, 4))
            count = max(0, 20 + deliveries * 18 - (day % 5) * 4 + rng.randint(-5, 5))
            w.writerow([day, weekday, deliveries, count, int(count < 5 and not deliveries)])

def train_stump(rows):
    """Fit a one-split binary decision tree by minimizing training misclassification."""
    best = None
    for feature in range(len(FEATURES)):
        values = sorted({x[feature] for x, _ in rows})
        for left, right in zip(values, values[1:]):
            threshold = (left + right) / 2
            for polarity in (0, 1):
                errors = 0
                for x, y in rows:
                    pred = int((x[feature] > threshold) == bool(polarity))
                    errors += pred != y
                candidate = (errors, feature, threshold, polarity)
                if best is None or candidate < best:
                    best = candidate
    if best is None:
        raise ValueError("training set lacks feature variation")
    return best[1:]

def run(path: Path) -> float:
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    dataset = [([int(row[k]) for k in FEATURES], int(row["stockout_next_day"])) for row in rows]
    split = int(len(dataset) * .75)
    train, test = dataset[:split], dataset[split:]
    feature, threshold, polarity = train_stump(train)
    predict = lambda x: int((x[feature] > threshold) == bool(polarity))
    accuracy = sum(predict(x) == y for x, y in test) / len(test)
    print(f"synthetic demonstration one-split-tree accuracy={accuracy:.3f}; not a field metric or allocation decision")
    return accuracy

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--synthetic", action="store_true")
    args = parser.parse_args()
    if not args.synthetic:
        parser.error("only --synthetic data is included; real data is not supplied")
    csv_path = ROOT / "synthetic.csv"
    make_data(csv_path)
    run(csv_path)
    csv_path.unlink()

"""Synthetic, nonvalidated logistic regression demo for inspection triage."""
import random

def main():
    rng = random.Random(7)
    points = [(rng.random(), rng.random()) for _ in range(100)]
    labels = [int(a > .75 or b > .8) for a, b in points]
    print(f"synthetic_rows={len(points)} positives={sum(labels)}; no model trained or accuracy claim")

if __name__ == "__main__": main()

import random
from statistics import mean

def generate(n=1000, seed=17):
    rng = random.Random(seed)
    return [(x, x + rng.gauss(0, 0.08) + (0.8 if i > n * 0.8 else 0)) for i, x in enumerate([1.0] * n)]

if __name__ == "__main__":
    values = generate()
    errors = [abs(observed - expected) for expected, observed in values]
    print(f"synthetic-only mean absolute deviation: {mean(errors):.4f}")

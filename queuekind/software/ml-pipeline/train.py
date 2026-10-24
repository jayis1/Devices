"""Generate synthetic arrival/completion counts and a simple wait-band baseline.
Synthetic output is for plumbing tests only, not evidence of prediction quality.
"""
import random

def generate(seed: int = 7, n: int = 100):
    rng = random.Random(seed)
    count = 0
    rows = []
    for _ in range(n):
        count = max(0, count + rng.choice((0, 0, 1, 1, -1)))
        rows.append((count, "under-15" if count < 3 else "15-30" if count < 8 else "over-30"))
    return rows

if __name__ == "__main__":
    rows = generate()
    print(f"synthetic rows={len(rows)} final_count={rows[-1][0]}; no real-world metric claimed")

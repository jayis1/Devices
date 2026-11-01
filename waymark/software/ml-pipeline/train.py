# Synthetic-only teaching example; no real-world performance claim.
import random

def samples(seed=7, count=200):
    random.seed(seed)
    return [(random.randint(0, 1), random.randint(0, 100)) for _ in range(count)]

if __name__ == '__main__':
    rows=samples()
    train=rows[:160]
    test=rows[160:]
    print(f"synthetic rows={len(rows)} train={len(train)} test={len(test)}")
    print("baseline=rule threshold quality<30; no accuracy metric (synthetic labels are not validated)")

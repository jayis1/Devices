"""Synthetic-only deterministic demonstration; not an operational ML model."""
import random

def make_demo(seed=7, count=20):
    rng = random.Random(seed)
    return [{"task": f"task-{i}", "household_member": f"member-{rng.randrange(3)}", "days_since_done": rng.randrange(1, 15)} for i in range(count)]

def suggest_oldest(tasks):
    return sorted(tasks, key=lambda row: row["days_since_done"], reverse=True)

if __name__ == "__main__":
    print("Synthetic demo only; no prediction or fairness claim.")
    print(suggest_oldest(make_demo())[:3])

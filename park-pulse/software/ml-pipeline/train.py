"""Synthetic maintenance triage example; not trained on field data."""
import random
def make_data(n=200):
    random.seed(7)
    return [(x:=random.random(), int(x > .85)) for _ in range(n)]
if __name__ == "__main__":
    data=make_data()
    print(f"synthetic samples={len(data)} positive={sum(y for _,y in data)}; no performance claim")

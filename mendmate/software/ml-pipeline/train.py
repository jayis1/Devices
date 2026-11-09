"""Synthetic-data pipeline smoke test, not fabric or repair performance evidence."""
import random

def main():
    rng=random.Random(7)
    data=[([rng.random() for _ in range(11)], rng.choice(('cotton','wool','unknown'))) for _ in range(30)]
    split=int(len(data)*0.8)
    train,test=data[:split],data[split:]
    majority=max({label:sum(1 for _,y in train if y==label) for label in {y for _,y in train}}.items(),key=lambda x:x[1])[0]
    correct=sum(y==majority for _,y in test)
    print(f"synthetic smoke: train={len(train)} test={len(test)} baseline_correct={correct}; NOT a validated classifier")
if __name__=='__main__': main()

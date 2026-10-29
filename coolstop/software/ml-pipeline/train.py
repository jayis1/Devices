import random


def main() -> None:
    rng = random.Random(17)
    rows = [(mass, int(mass < 500)) for mass in (rng.uniform(0, 1200) for _ in range(500))]
    split = 400
    train, test = rows[:split], rows[split:]
    # A transparent baseline: learn the sample median boundary, no external ML library.
    boundary = sorted(mass for mass, _ in train)[len(train) // 2]
    accuracy = sum(int((mass < boundary) == bool(label)) for mass, label in test) / len(test)
    print(f"synthetic_holdout_accuracy={accuracy:.3f}; threshold_g={boundary:.1f}; NOT VALIDATED")


if __name__ == "__main__":
    main()

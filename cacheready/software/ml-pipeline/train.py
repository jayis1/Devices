# Synthetic demonstration only; no real cache data or performance claims.
def train():
    # Baseline rule: flag an inspection older than the configured interval.
    samples = [(age, age > 7) for age in range(15)]
    predicted = [age > 7 for age, _ in samples]
    assert predicted == [label for _, label in samples]
    print(f"synthetic samples={len(samples)}; rule baseline checked (not a trained model)")

if __name__ == "__main__":
    train()

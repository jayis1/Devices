"""Synthetic drift-baseline smoke test; deliberately not an access classifier."""
import random
random.seed(7)
reference = [1000 + random.randint(-5, 5) for _ in range(100)]
maintenance = [x + (20 if i >= 50 else 0) for i, x in enumerate(reference)]
threshold = 15
alerts = sum(abs(x - r) > threshold for x, r in zip(maintenance, reference))
print(f"synthetic_samples={len(reference)} synthetic_drift_flags={alerts}")
print("Synthetic demonstration only; no model is trained and no real access state is inferred.")

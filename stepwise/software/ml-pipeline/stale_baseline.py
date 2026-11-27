"""Experimental synthetic-fixture baseline; not a safety model."""
def stale(age_minutes: float, threshold_minutes: float = 60.0) -> bool:
    if age_minutes < 0:
        raise ValueError("age cannot be negative")
    return age_minutes > threshold_minutes

if __name__ == "__main__":
    print("Synthetic demo only:", stale(75.0))

"""Synthetic demonstration; date priority only, no trained predictive model."""
import argparse
from datetime import date

def priority(expiry: str | None, today: date | None = None) -> int:
    if not expiry: return 0
    delta = (date.fromisoformat(expiry) - (today or date.today())).days
    return 2 if delta <= 30 else 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    print("Synthetic baseline ready; no model trained and no real-world accuracy established.")

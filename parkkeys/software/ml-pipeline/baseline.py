# Deterministic synthetic maintenance-gap baseline; not an ML model or predictive claim.
def overdue(last_checked_days: float, threshold_days: float = 7.0) -> bool:
    if last_checked_days < 0:
        raise ValueError("age must be nonnegative")
    return last_checked_days >= threshold_days

if __name__ == "__main__":
    for age in (1.0, 7.0, 10.0):
        print(f"age_days={age:g}, steward_check_suggested={overdue(age)}")

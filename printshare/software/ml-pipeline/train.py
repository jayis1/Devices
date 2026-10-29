"""Baseline only: deterministic rule for stale/unknown status; no trained model."""
def readiness(last_seen_seconds: int, printer_state: int, humidity_centi_rh: int) -> str:
    if last_seen_seconds > 600 or printer_state not in range(1, 5):
        return "unknown"
    if printer_state in (3, 4):
        return "attention"
    if humidity_centi_rh > 6000:
        return "filament-check"
    return "observed"

if __name__ == "__main__":
    print(readiness(30, 1, 4500))

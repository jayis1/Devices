"""Record a manual flow reference; hardware calibration remains site-specific."""
def liters_per_pulse(liters: float, pulses: int) -> float:
    if liters <= 0 or pulses <= 0: raise ValueError("positive reference required")
    return liters / pulses

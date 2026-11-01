"""Threshold baseline on synthetic example rows; not a trained instrument model."""

def outside_band(temp_c: float, rh_pct: float) -> bool:
    return not (-5.0 <= temp_c <= 40.0 and 25.0 <= rh_pct <= 70.0)

if __name__ == "__main__":
    demo = [(21.0, 48.0), (42.0, 72.0)]
    print([outside_band(t, h) for t, h in demo])

"""Transparent capacity estimate; requires user-provided calibrated geometry."""
def volume_litres(distance_mm: float, tank_height_mm: float, capacity_l: float) -> float:
    if not (0 <= distance_mm <= tank_height_mm and tank_height_mm > 0 and capacity_l > 0):
        raise ValueError("invalid geometry or reading")
    return capacity_l * (1.0 - distance_mm / tank_height_mm)

if __name__ == "__main__":
    print("Prototype only: provide calibrated measurements before using volume_litres.")

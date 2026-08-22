from __future__ import annotations

from datetime import datetime, timedelta, timezone
import math

STREAMS = [
    "unknown",
    "recycle",
    "compost",
    "landfill",
    "glass",
    "deposit",
    "special_dropoff",
]


def resolve_sort(barcode: str | None, rgb_features: list[float], spectral: list[float], municipality: str) -> dict:
    rgb_mean = sum(rgb_features) / len(rgb_features) if rgb_features else 0.0
    spectral_mean = sum(spectral) / len(spectral) if spectral else 0.0
    confidence = max(0.25, min(0.98, 0.45 + rgb_mean * 0.30 + spectral_mean / 1000.0))

    if barcode and municipality.startswith("us-") and confidence > 0.70:
        stream = "recycle"
        material = "pet_1_clear"
        instructions = "Rinse lightly and replace cap before recycling."
    elif spectral_mean > 100:
        stream = "compost"
        material = "food_soiled_fiber"
        instructions = "Place in compost; remove any plastic liner first."
    else:
        stream = "landfill"
        material = "mixed_residual"
        instructions = "This item is likely residual waste in your current rule pack."

    return {
        "stream": stream,
        "material": material,
        "confidence": round(confidence, 3),
        "instructions": instructions,
    }


def diversion_score(bin_rows: list[dict]) -> float:
    total = sum(max(row.get("mass_grams", 0), 0) for row in bin_rows) or 1
    diverted = sum(
        max(row.get("mass_grams", 0), 0)
        for row in bin_rows
        if row.get("stream") in {"recycle", "compost", "glass", "deposit"}
    )
    return round(100.0 * diverted / total, 2)


def fill_forecast(current_fill_pct: int, hours_to_pickup: int) -> float:
    projected = current_fill_pct + (hours_to_pickup / 24.0) * 11.5
    return round(min(100.0, projected), 1)


def pickup_forecast(current_fill_pct: int, curb_placed: bool) -> dict:
    next_pickup = datetime.now(timezone.utc) + timedelta(hours=18)
    overflow = min(0.99, 0.08 + current_fill_pct / 125.0)
    miss = 0.12 if curb_placed else 0.42 + math.sin(current_fill_pct / 50.0) * 0.05
    return {
        "next_pickup": next_pickup,
        "overflow_risk": round(float(overflow), 3),
        "miss_risk": round(max(0.01, min(0.99, float(miss))), 3),
        "curb_placed": curb_placed,
    }


def recommendations(bin_rows: list[dict]) -> list[dict]:
    results: list[dict] = []
    for row in bin_rows:
        if row.get("fill_pct", 0) > 85:
            results.append({
                "kind": "pickup",
                "title": f"{row['stream'].title()} bin nearly full",
                "detail": "Place curb cart out early; overflow risk is elevated before the next service window.",
                "priority": 3,
            })
        if row.get("voc_index", 0) > 160:
            results.append({
                "kind": "maintenance",
                "title": f"High odor detected in {row['stream']}",
                "detail": "Run deodorize cycle and add dry browns or absorbent paper.",
                "priority": 2,
            })
    if not results:
        results.append({
            "kind": "tip",
            "title": "Diversion stable",
            "detail": "Your household is sorting consistently. Consider adding a film-plastic drop-off routine next.",
            "priority": 1,
        })
    return results

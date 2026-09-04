from __future__ import annotations


def clamp(value: float, low: float = 0.0, high: float = 0.99) -> float:
    return max(low, min(high, round(value, 3)))


def contamination_risk(latest: dict[str, dict]) -> float:
    water = latest.get("water_quality", {}).get("metrics", {})
    weather = latest.get("weather", {}).get("metrics", {})
    turbidity = float(water.get("turbidity_ntu", 0.0))
    orp = float(water.get("orp_mv", 220.0))
    rainfall = float(weather.get("rain_mm", 0.0))
    ph = float(water.get("ph", 7.0))
    score = 0.08 + turbidity * 0.11 + max(0.0, (210.0 - orp) / 300.0) + rainfall / 400.0 + max(0.0, 6.5 - ph) * 0.08
    return clamp(score)


def pump_failure_risk(latest: dict[str, dict]) -> float:
    pump = latest.get("pump_state", {}).get("metrics", {})
    starts = float(pump.get("starts_per_hour", 0.0))
    cavitation = float(pump.get("cavitation_score", 0.0))
    pressure_rise = float(pump.get("pressure_rise_kpa", 180.0))
    score = 0.05 + starts / 30.0 + cavitation / 130.0 + max(0.0, 160.0 - pressure_rise) / 220.0
    return clamp(score)


def dry_well_risk(latest: dict[str, dict]) -> float:
    pump = latest.get("pump_state", {}).get("metrics", {})
    weather = latest.get("weather", {}).get("metrics", {})
    dry_run = float(pump.get("dry_run_score", 0.0))
    dry_spell = float(weather.get("dry_spell_days", 0.0))
    deep_soil = float(weather.get("soil_deep_pct", 50.0))
    score = 0.04 + dry_run / 120.0 + dry_spell / 30.0 + max(0.0, 45.0 - deep_soil) / 70.0
    return clamp(score)


def treatment_integrity_risk(latest: dict[str, dict]) -> float:
    tap = latest.get("tap_event", {}).get("metrics", {})
    uv_lux = float(tap.get("uv_lux", 800.0))
    filter_days = float(tap.get("filter_days_remaining", 90.0))
    score = 0.03 + max(0.0, 500.0 - uv_lux) / 650.0 + max(0.0, 21.0 - filter_days) / 60.0
    return clamp(score)


def derive_state(latest: dict[str, dict]) -> tuple[str, list[str]]:
    risks = {
        "contamination": contamination_risk(latest),
        "pump_failure": pump_failure_risk(latest),
        "dry_well": dry_well_risk(latest),
        "treatment_integrity": treatment_integrity_risk(latest),
    }
    advisories: list[str] = []
    state = "safe"
    if risks["dry_well"] > 0.85 or risks["contamination"] > 0.8:
        state = "do_not_drink"
    elif max(risks.values()) > 0.58:
        state = "treat"
    elif max(risks.values()) > 0.34:
        state = "watch"

    if risks["contamination"] > 0.58:
        advisories.append("Collect a confirmation sample and avoid untreated drinking water.")
    if risks["pump_failure"] > 0.5:
        advisories.append("Inspect pressure tank cycling and pump current signature.")
    if risks["dry_well"] > 0.45:
        advisories.append("Reduce discretionary use and monitor recovery overnight.")
    if risks["treatment_integrity"] > 0.45:
        advisories.append("Service UV lamp or replace under-sink cartridge.")
    return state, advisories


def explain_risks(latest: dict[str, dict]) -> dict[str, str]:
    return {
        "contamination": "Driven by turbidity, ORP, rainfall, and pH drift.",
        "pump_failure": "Driven by starts per hour, pressure rise, and cavitation score.",
        "dry_well": "Driven by dry-run score, long dry spells, and deep soil moisture.",
        "treatment_integrity": "Driven by UV indicator brightness and filter service window.",
    }

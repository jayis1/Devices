from __future__ import annotations

from collections import Counter

RESIDUE_SEVERITY = {
    "clean": 0,
    "dust": 20,
    "soap": 32,
    "grease": 48,
    "biofilm": 62,
    "mildew-risk": 55,
}

def cleanliness_score(rooms: list[dict], scans: list[dict]) -> int:
    if not rooms:
        return 92
    room_penalty = sum(r.get("dust_index", 0) * 0.35 + r.get("traffic_score", 0) * 0.15 + r.get("wet_floor_probability", 0) * 35 for r in rooms) / max(len(rooms), 1)
    scan_penalty = sum(RESIDUE_SEVERITY.get(s.get("residue_class", "clean"), 25) for s in scans[-10:]) / max(min(len(scans), 10), 1) if scans else 0
    score = int(round(100 - min(85, room_penalty * 0.6 + scan_penalty * 0.4)))
    return max(5, min(100, score))


def active_alerts(rooms: list[dict], dock: dict | None) -> list[str]:
    alerts: list[str] = []
    for room in rooms:
        if room.get("wet_floor_probability", 0) >= 0.7:
            alerts.append(f"Wet-floor risk in {room['room']}")
        if room.get("humidity_pct", 0) >= 70 and room.get("room") in {"bathroom", "laundry"}:
            alerts.append(f"High mildew risk in {room['room']}")
    if dock:
        if dock.get("leak_detected"):
            alerts.append("Dock leak detected")
        if dock.get("detergent_ml", 0) < 75:
            alerts.append("Detergent low")
    return alerts


def recommendations(rooms: list[dict], scans: list[dict], dock: dict | None) -> list[dict]:
    items: list[dict] = []
    ranked = sorted(rooms, key=lambda r: (r.get("dust_index", 0) + r.get("traffic_score", 0) + int(r.get("wet_floor_probability", 0) * 100)), reverse=True)
    for room in ranked[:3]:
        items.append({
            "title": f"Clean {room['room']} next",
            "detail": f"Dust {room.get('dust_index', 0)}, traffic {room.get('traffic_score', 0)}, wet-risk {room.get('wet_floor_probability', 0):.0%}",
            "priority": 5 if room.get("wet_floor_probability", 0) > 0.7 else 4,
        })
    if dock and dock.get("detergent_ml", 0) < 120:
        items.append({"title": "Refill dock detergent", "detail": "Projected depletion within one week.", "priority": 4})
    residue_counts = Counter(scan.get("residue_class", "clean") for scan in scans[-8:])
    if residue_counts.get("grease", 0) >= 2:
        items.append({"title": "Run kitchen degrease routine", "detail": "Repeated grease detections on recent wand scans.", "priority": 4})
    return items[:5]


def supply_forecast(dock: dict | None) -> dict:
    if not dock:
        return {"detergent_days": 9, "brush_days": 22, "bag_days": 15}
    detergent_days = max(1, int(dock.get("detergent_ml", 200) / 25))
    return {"detergent_days": detergent_days, "brush_days": 21, "bag_days": 16}


def optimize_schedule(rooms: list[dict], available_windows: list[str]) -> tuple[str, str, list[str]]:
    if not available_windows:
        available_windows = ["09:00"]
    ranked = sorted(rooms, key=lambda r: (r.get("wet_floor_probability", 0), r.get("dust_index", 0), r.get("traffic_score", 0)), reverse=True)
    target_rooms = [r["room"] for r in ranked[:2]] or ["kitchen"]
    window = available_windows[0]
    reason = "Highest dirt and wet-floor risk rooms clustered into a single efficient mission."
    return window, reason, target_rooms

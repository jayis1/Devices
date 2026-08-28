from __future__ import annotations

from datetime import datetime, timedelta, timezone

from models import DepartureRequest, FocusRequest


def evaluate_departure(request: DepartureRequest) -> dict:
    risk = 0.12
    risk += 0.17 * request.missing_items
    risk += 0.18 if request.minutes_to_deadline < 10 else 0.05
    risk += 0.14 if request.door_open else 0.02
    risk += min(request.tray_mass_delta / 1000.0, 0.25)
    risk += 0.20 * request.focus_fragmentation
    risk = round(min(risk, 0.99), 3)

    likely_missing = "keys"
    if request.tray_mass_delta > 130:
        likely_missing = "laptop"
    elif request.missing_items >= 2:
        likely_missing = "wallet"

    if risk > 0.70:
        nudge = "tag_chirp"
    elif risk > 0.50:
        nudge = "epaper_prompt"
    elif risk > 0.30:
        nudge = "led_shift"
    else:
        nudge = "none"

    return {
        "miss_risk": risk,
        "likely_missing_item": likely_missing,
        "nudge": nudge,
        "detail": f"{likely_missing.title()} last seen in office if office routine was active.",
        "confidence": round(0.62 + min(0.3, request.missing_items * 0.08), 3),
    }


def find_item(item_name: str, last_seen_room: str, minutes_since_seen: int, movement_events: int, doorway_seen: bool) -> dict:
    base_room = last_seen_room or "office"
    confidence = 0.81 if minutes_since_seen < 60 else 0.61
    if movement_events > 4:
        confidence -= 0.12
    if doorway_seen:
        base_room = "entryway"
        confidence += 0.08

    search_order = [base_room, "office", "bedroom", "kitchen", "car"]
    deduped = []
    for room in search_order:
        if room not in deduped:
            deduped.append(room)

    return {
        "item_name": item_name,
        "best_room": base_room,
        "confidence": round(max(0.2, min(confidence, 0.97)), 3),
        "search_order": deduped,
    }


def evaluate_focus(request: FocusRequest) -> dict:
    if request.session_minutes > 95 and request.seat_exits == 0:
        return {
            "focus_state": "hyperfocus-risk",
            "cue": "tone",
            "explanation": "Long uninterrupted session suggests a transition prompt is warranted.",
        }
    if request.seat_exits > 4 or request.noise_db > 62:
        return {
            "focus_state": "distracted",
            "cue": "led_shift",
            "explanation": "Frequent exits or elevated noise indicates fragmented attention.",
        }
    if request.session_minutes > 50 or request.low_light:
        return {
            "focus_state": "transition-needed",
            "cue": "haptic",
            "explanation": "Session duration or environment strain suggests a guided break.",
        }
    return {
        "focus_state": "focused",
        "cue": "none",
        "explanation": "Current session looks stable; hold notifications.",
    }


def overview_payload() -> dict:
    now = datetime.now(timezone.utc)
    return {
        "household": "demo-home",
        "generated_at": now,
        "active_routine": "workday",
        "departure_risk": 0.42,
        "focus_state": "focused",
        "missing_items": ["laptop"],
        "tags_online": 6,
        "recommendations": [
            "Stage laptop sleeve on the doorway dock before 8:00 PM.",
            "Shift school pickup cue 7 minutes earlier on Fridays.",
        ],
    }


def routine_statuses() -> list[dict]:
    now = datetime.now(timezone.utc)
    return [
        {
            "name": "workday",
            "completion_rate": 0.86,
            "average_slip_minutes": 6.5,
            "next_due": now + timedelta(hours=12),
        },
        {
            "name": "school",
            "completion_rate": 0.91,
            "average_slip_minutes": 4.0,
            "next_due": now + timedelta(hours=16),
        },
    ]

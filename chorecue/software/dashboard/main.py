"""Local event-contract validation only; no HTTP server or MQTT client implemented."""

LIMITS = {
    "task_done": (0, 1, "bool"),
    "supply_g": (0, 100000, "g"),
    "availability": (0, 2, "enum"),
}

def validate_event(event):
    if not isinstance(event, dict) or set(event) != {"v", "node", "seq", "type", "value", "unit", "utc_ms"}:
        return False
    if type(event["v"]) is not int or event["v"] != 1:
        return False
    node = event["node"]
    if not isinstance(node, str) or not 1 <= len(node) <= 48 or not node.isascii():
        return False
    if type(event["seq"]) is not int or not 0 <= event["seq"] <= 0xffffffff:
        return False
    if type(event["utc_ms"]) is not int or not 0 <= event["utc_ms"] <= 0xffffffffffffffff:
        return False
    kind = event["type"]
    if kind not in LIMITS:
        return False
    low, high, unit = LIMITS[kind]
    return type(event["value"]) is int and low <= event["value"] <= high and event["unit"] == unit

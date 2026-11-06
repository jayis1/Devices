import json
from pathlib import Path

def main():
    # Schema/serialization smoke only; not a trained or evaluated predictive model.
    event = {"v": 1, "node": "serve-01", "seq": 1, "ts": "2026-01-01T12:00:00Z", "kind": "probe_reading", "value": 72.4, "unit": "C", "quality": "staff_entered"}
    assert json.loads(json.dumps(event))["unit"] == "C"
    print("synthetic protocol fixture passed; no ML accuracy claim")

if __name__ == "__main__":
    main()

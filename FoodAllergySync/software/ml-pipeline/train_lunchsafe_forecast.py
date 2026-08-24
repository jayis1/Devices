from __future__ import annotations

from pathlib import Path
import json


def main() -> None:
    artifact = {
        "model": "baseline_lunchsafe_sequence",
        "features": ["temp_curve", "lid_open_events", "meal_type", "handoff_time"],
        "objective": "minutes_to_temp_threshold",
    }
    Path("artifacts").mkdir(exist_ok=True)
    Path("artifacts/lunchsafe_forecast.json").write_text(json.dumps(artifact, indent=2))
    print(artifact)


if __name__ == "__main__":
    main()

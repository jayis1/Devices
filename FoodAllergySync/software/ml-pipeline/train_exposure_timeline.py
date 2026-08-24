from __future__ import annotations

from pathlib import Path
import json


def main() -> None:
    artifact = {
        "model": "baseline_exposure_timeline_transformer",
        "horizon_days": 7,
        "signals": [
            "package_alerts",
            "strip_results",
            "lunch_temp_events",
            "epipen_readiness",
        ],
    }
    Path("artifacts").mkdir(exist_ok=True)
    Path("artifacts/exposure_timeline.json").write_text(json.dumps(artifact, indent=2))
    print(artifact)


if __name__ == "__main__":
    main()

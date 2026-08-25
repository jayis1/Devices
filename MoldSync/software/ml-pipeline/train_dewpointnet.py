from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pandas as pd


def main() -> None:
    rng = np.random.default_rng(42)
    hours = np.arange(0, 24 * 14)
    rh = 55 + 20 * np.sin(hours / 12.0) + rng.normal(0, 4, size=hours.size)
    surface_margin = 3.0 - 0.06 * (rh - 55) + rng.normal(0, 0.35, size=hours.size)
    df = pd.DataFrame({"hour": hours, "rh": rh, "surface_margin": surface_margin})
    artifact = {
        "model": "DewPointNet-GRU-baseline",
        "rows": int(len(df)),
        "mean_margin": round(float(df["surface_margin"].mean()), 3),
        "min_margin": round(float(df["surface_margin"].min()), 3),
    }
    out = Path(__file__).resolve().parents[2] / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'dewpointnet_report.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))


if __name__ == '__main__':
    main()

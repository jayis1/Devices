from pathlib import Path
import json
import numpy as np

rng = np.random.default_rng(46)
protocols = []
for idx in range(128):
    pain = float(rng.uniform(3, 9))
    flow = float(rng.uniform(0, 10))
    temp = float(rng.uniform(38, 43))
    haptic = int(rng.integers(0, 2))
    relief_gain = max(0.0, min(1.0, 0.18 + 0.09 * haptic + 0.03 * (temp - 38) + rng.normal(0, 0.08)))
    protocols.append({
        "ctx_pain": round(pain, 2),
        "ctx_flow": round(flow, 2),
        "target_temp_c": round(temp, 2),
        "haptics": bool(haptic),
        "reward": round(relief_gain, 4),
    })
out = Path(__file__).resolve().parent / "artifacts"
out.mkdir(exist_ok=True)
path = out / "relief_policy.json"
path.write_text(json.dumps(protocols, indent=2), encoding="utf-8")
print({"model": "relief_policy", "rows": len(protocols), "artifact": str(path)})

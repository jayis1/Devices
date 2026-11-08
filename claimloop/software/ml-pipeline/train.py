import argparse, json, random
from pathlib import Path

parser = argparse.ArgumentParser(description="Pipeline plumbing smoke test; synthetic labels are not evidence")
parser.add_argument("--output", default="claimloop-model.json")
args = parser.parse_args()
rng = random.Random(2026)
rows = [{"weight_g": rng.randrange(0, 8000), "category": "synthetic"} for _ in range(100)]
report = {"status": "synthetic-smoke-test-only", "seed": 2026, "rows": len(rows), "model": None,
          "note": "No representative, consented dataset; no classifier trained or accuracy claimed."}
Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report))

import json
from pathlib import Path
rows=[json.loads(line) for line in Path("sample.jsonl").read_text().splitlines()]
correct=sum(r["care_prompt"] == int(r["days_since_worn"]>14) for r in rows)
print(f"rows={len(rows)} baseline_rule_agreement={correct/len(rows):.3f}; synthetic smoke test only, not predictive validation")

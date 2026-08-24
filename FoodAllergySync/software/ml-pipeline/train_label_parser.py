from __future__ import annotations

from pathlib import Path
import json

DATA = [
    {"text": "contains milk and soy", "label": "unsafe"},
    {"text": "processed on shared equipment with peanuts", "label": "caution"},
    {"text": "rice, salt, olive oil", "label": "safe"},
]


def main() -> None:
    out = {
        "model": "rule_bootstrap_label_parser",
        "samples": len(DATA),
        "classes": sorted({row["label"] for row in DATA}),
    }
    Path("artifacts").mkdir(exist_ok=True)
    Path("artifacts/label_parser.json").write_text(json.dumps(out, indent=2))
    print(out)


if __name__ == "__main__":
    main()

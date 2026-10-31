import json
import random
from pathlib import Path

# Synthetic demonstration only. No real site dataset or predictive claim.
def main():
    random.seed(7)
    samples = [{"sound_level": random.randint(20, 70), "stock": random.randint(0, 12),
                "label": "check" if random.random() < 0.1 else "ready"}
               for _ in range(100)]
    out = Path(__file__).with_name("synthetic-example.json")
    out.write_text(json.dumps({"provenance": "synthetic; not for deployment", "samples": samples}, indent=2))
    print(f"wrote {len(samples)} synthetic examples to {out.name}")

if __name__ == "__main__":
    main()

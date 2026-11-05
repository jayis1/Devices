"""Synthetic smoke-test data only; no real-world validation or safety use."""
import csv, random
from pathlib import Path
def main():
    r=random.Random(7)
    rows=[{"weight_delta_g":r.randint(-1500,1500),"hours_since_check":r.randint(0,240),"review_suggested":1} for _ in range(100)]
    out=Path("synthetic_events.csv")
    with out.open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=rows[0]); w.writeheader(); w.writerows(rows)
    print(f"wrote {len(rows)} synthetic examples to {out}; not a trained model")
if __name__=="__main__": main()

# ML demo: explicit-event aggregation only; no synthetic efficacy claims.
import csv, random
from pathlib import Path
random.seed(17)
def demo_rows(n=40):
    return [{'day': i//4, 'sessions': random.randrange(0,3), 'minutes': random.randrange(0,31)} for i in range(n)]
def main():
    rows=demo_rows()
    out=Path('demo_events.csv')
    with out.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
    print(f'Wrote {len(rows)} synthetic demonstration rows to {out}; not suitable for model evaluation.')
if __name__=='__main__': main()

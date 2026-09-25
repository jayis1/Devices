"""Train a transparent room-zone confidence model from consented UWB ranges."""
from __future__ import annotations
import argparse, csv, math, random
from pathlib import Path

def rows(synthetic: bool):
    if synthetic:
        random.seed(42)
        return [(random.uniform(50, 4000), 1 if random.random() < math.exp(-random.uniform(50,4000)/1200) else 0) for _ in range(200)]
    raise SystemExit("Provide consented labeled data in a production pipeline; use --synthetic only for a demo.")

def main() -> None:
    parser=argparse.ArgumentParser(); parser.add_argument('--synthetic', action='store_true'); parser.add_argument('--output', default='zone_model.csv'); args=parser.parse_args()
    samples=rows(args.synthetic); threshold=sum(x for x,y in samples if y)/max(1,sum(y for _,y in samples))
    with Path(args.output).open('w', newline='') as f: csv.writer(f).writerows([['feature','value'],['range_cm_threshold',round(threshold,2)],['training_data','synthetic' if args.synthetic else 'consented']])
    print(f'wrote {args.output}')
if __name__ == '__main__': main()

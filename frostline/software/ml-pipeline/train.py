import argparse
import json
import random
from pathlib import Path

def make_samples(count, seed):
    rng=random.Random(seed)
    rows=[]
    for _ in range(count):
        temp=rng.uniform(-8,8); wet=rng.randrange(0,2)
        rows.append({'surface_temp_c':round(temp,2),'wetness':wet,'inspection_priority':int(wet and temp<=2)})
    return rows

def main():
    p=argparse.ArgumentParser(); p.add_argument('--count',type=int,default=100); p.add_argument('--seed',type=int,default=11); p.add_argument('--out',default='/tmp/frostline-synthetic.json')
    a=p.parse_args(); rows=make_samples(a.count,a.seed)
    Path(a.out).write_text(json.dumps({'synthetic_only':True,'samples':rows},indent=2))
    print(f'Wrote {len(rows)} synthetic smoke-test samples to {a.out}; no model trained.')
if __name__=='__main__': main()
